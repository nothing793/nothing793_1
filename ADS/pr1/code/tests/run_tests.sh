#!/usr/bin/env bash
# pr1 端到端测试：在临时工作目录里编译并跑完整流水线。
# 用法：sh run_tests.sh [工作目录]        （默认 /tmp/pr1-test）
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TESTS=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-/tmp/pr1-test}

pass=0
fail=0
ok()   { printf '  PASS  %s\n' "$1"; pass=$((pass + 1)); }
bad()  { printf '  FAIL  %s\n' "$1"; fail=$((fail + 1)); }

echo "== 工作目录: $WORK"
rm -rf "$WORK"
mkdir -p "$WORK" || exit 1
cd "$WORK" || exit 1

CFLAGS="-std=c99 -Wall -Wextra -Wpedantic"

echo "== 1. 编译（要零告警）"
if ! gcc $CFLAGS -o index_gen "$ROOT/index_gen.c" "$ROOT/stem.c" -lm 2>build.log; then
  echo "  FAIL  index_gen 编译失败"; cat build.log; exit 1
fi
if ! gcc $CFLAGS -o query "$ROOT/query.c" "$ROOT/stem.c" -lm 2>>build.log; then
  echo "  FAIL  query 编译失败"; cat build.log; exit 1
fi
if ! gcc $CFLAGS -o stem_list "$TESTS/stem_list.c" "$ROOT/stem.c" -lm 2>>build.log; then
  echo "  FAIL  stem_list 编译失败"; cat build.log; exit 1
fi
if ! gcc $CFLAGS -o roundtrip "$TESTS/roundtrip.c" "$ROOT/stem.c" -lm 2>>build.log; then
  echo "  FAIL  roundtrip 编译失败"; cat build.log; exit 1
fi
if [ -s build.log ]; then
  bad "编译有告警：" ; cat build.log
else
  ok "四个程序编译零告警（交付的两个源文件只有 index_gen.c / query.c + 外部引用 stem.c）"
fi

echo "== 2. 生成合成语料 + 切词（index_gen --dump-tokens 写出三元组）"
python3 "$TESTS/gen_corpus.py" corpus --docs 40 --words 4000 --seed 7 > gen.log 2>&1 && ok "语料生成（$(cat gen.log)）" || { bad "语料生成"; cat gen.log; }
./index_gen --count-only --dump-tokens=file.txt corpus/*.txt >/dev/null 2>tokenize.log && ok "切词 -> file.txt（$(wc -l < file.txt) 条三元组，--count-only 只跑 Part 1）" || { bad "切词"; cat tokenize.log; }

echo "== 3. 切词口径对拍（tr -cs 'A-Za-z0-9'）"
awk '{print $1}' file.txt | sort > got_words.txt
tr -cs 'A-Za-z0-9' '\n' < corpus/doc_000.txt | tr 'A-Z' 'a-z' | sed '/^$/d' | sort > want_words.txt
echo "doc_000 的词表" >/dev/null
# 只对第 0 篇做逐词对拍（doc_id 0 的三元组）
awk '$2 == 0 {print $1}' file.txt | sort > got_doc0.txt
if diff -q want_words.txt got_doc0.txt >/dev/null; then
  ok "doc_000 切词与 tr -cs 逐词一致（$(wc -l < got_doc0.txt) 个词）"
else
  bad "doc_000 切词与 tr -cs 不一致"; diff want_words.txt got_doc0.txt | head -5
fi

echo "== 3b. 从 stdin 读一篇文档（-）"
if ./index_gen --count-only --dump-tokens=stdin_doc0.txt - < corpus/doc_000.txt >/dev/null 2>stdin.log \
   && awk '$2 == 0' file.txt > want_doc0.txt \
   && diff -q want_doc0.txt stdin_doc0.txt >/dev/null; then
  ok "stdin 一篇文档切词结果与 corpus/doc_000.txt 一致（$(wc -l < stdin_doc0.txt) 条三元组）"
else
  bad "stdin 切词与文件切词不一致"; head -3 stdin.log
fi

echo "== 4. 建索引（Part 1 统计 + Part 2 建索引，θ = 0.5）"
./index_gen --theta=0.5 corpus/*.txt 2>index_gen.log && ok "index_gen 退出 0（$(grep -c . index_gen.log) 行统计）" || { bad "index_gen"; cat index_gen.log; }
sed -n '1p;2p;3p' index_gen.log | sed 's/^/        /'
cp index.bin index.good.bin
ok "$(grep 'stoplist' index_gen.log | tail -1 | sed 's/^/统计: /')"
if [ "$(grep -c -v '^#' stoplist.txt)" -gt 0 ]; then
  ok "stoplist.txt 有 $(grep -c -v '^#' stoplist.txt) 个停用词"
else
  bad "stoplist.txt 里一个停用词都没有（合成语料的 θ 可能不合适）"
fi

echo "== 5. 独立对拍：index.bin vs file.txt 暴力聚合"
if python3 "$TESTS/brute_check.py" --index index.bin --tokens file.txt --stemmer ./stem_list --stoplist stoplist.txt > brute.log 2>&1; then
  ok "$(head -1 brute.log)"
else
  bad "对拍失败"; head -3 brute.log
fi

echo "== 6. 往返：load 再 save 必须逐字节相同"
./roundtrip index.bin roundtrip.bin >/dev/null 2>&1 && cmp -s index.bin roundtrip.bin && ok "index.bin == roundtrip(index.bin)" || bad "往返后与原文件不同"

echo "== 7. 短语查询对拍"
PHRASE="zolkim vireth qandel brusett nomin"
python3 "$TESTS/brute_check.py" --index index.bin --tokens file.txt --stemmer ./stem_list --stoplist stoplist.txt --phrase "$PHRASE" > phrase.log 2>&1 || bad "短语暴力枚举失败"
grep '^PHRASE' phrase.log | sed 's/^/        /'
./query "$PHRASE" >/dev/null 2>&1 || bad "短语查询退出码非 0"
awk -F'[, ]+' '/^Doc ID: [0-9]+, Positions:/ {for (i=5;i<=NF;i++) print $3" "$i}' output.txt | sort > got_phrase.txt
grep -v -e '^PHRASE' -e '^PASS' phrase.log | sort > want_phrase.txt
if [ -s want_phrase.txt ] && diff -q want_phrase.txt got_phrase.txt >/dev/null; then
  ok "短语 \"$PHRASE\" 的 (doc, start) 与暴力枚举一致（$(wc -l < want_phrase.txt) 处）"
else
  bad "短语坐标不一致"; echo "        want: $(head -3 want_phrase.txt | tr '\n' ' ')"; echo "        got : $(head -3 got_phrase.txt | tr '\n' ' ')"
fi

echo "== 7b. 含停用词的短语（索引里没有这些词，必然查不到）"
./query "to be or not to be" >/dev/null 2>&1
if grep -q '^Not found: be ' output.txt && grep -q '^Phrase \[to be or not to be\]: 0 match' output.txt; then
  ok "短语含停用词 -> 稳定查不到（这是 Part 2 剔除停用词的必然副作用，报告里要写明）"
else
  bad "含停用词的短语行为不符合预期"; head -3 output.txt
fi
echo "== 8. 停用词真的不在索引里，且 query 能解释 Not found"
STOP=$(grep -v '^#' stoplist.txt | head -1 | awk '{print $1}')
if grep -q "Not found: $STOP  (stop word:" output.txt || ./query "$STOP" >/dev/null 2>&1; then
  ./query "$STOP" >/dev/null 2>&1
  if grep -q "Not found: $STOP  (stop word:" output.txt; then
    ok "查停用词 '$STOP' -> 提示是 part 1 剔除的停用词"
  else
    bad "查停用词 '$STOP' 没有给出停用词提示"; head -2 output.txt
  fi
  if python3 - "$STOP" <<'PYEOF'
import sys
stem = sys.argv[1]
with open('stoplist.txt') as fh:
    stops = {l.split()[0] for l in fh if not l.startswith('#') and l.split()}
sys.exit(0 if stem in stops else 1)
PYEOF
  then ok "stoplist.txt 与查询提示一致"; else bad "stoplist.txt 与查询提示不一致"; fi
else
  bad "无法用 query 验证停用词"
fi

echo "== 9. 阈值 τ 的作用"
MID=$(python3 "$TESTS/pick_term.py" --tokens file.txt --stemmer ./stem_list --min 0.15 --max 0.45)
MIDSTEM=$(echo "$MID" | awk '{print $1}')
echo "        选一个 df/N 在 (0.15, 0.45] 的词: $MID"
./query "$MIDSTEM" >/dev/null 2>&1
big=$(grep -c '^Doc ID' output.txt)
./query --tau=0.2 "$MIDSTEM" >/dev/null 2>&1
if grep -q "^Too common: $MIDSTEM " output.txt; then
  ok "τ = 0.2 时 '$MIDSTEM' 被拦下（不设阈值时输出了 $big 行 Doc ID）"
else
  bad "τ = 0.2 没有拦下 '$MIDSTEM'"; head -2 output.txt
fi
./query sword crown >/dev/null 2>&1
grep -E '^(Found|Not found|AND \[|Doc ID)' output.txt | head -6 | sed 's/^/        /'
echo "== 10. 损坏的索引文件必须被拒绝"
damage() {  # $1 = 描述，$2 = 破坏 index.bin 的命令
  cp index.good.bin index.bin
  eval "$2"
  if ./query the >/dev/null 2>&1; then bad "$1（query 竟然接受了）"; else ok "$1（query 拒绝并报错）"; fi
}
damage "截断 1 字节" 'head -c $(( $(stat -c%s index.bin) - 1 )) index.good.bin > index.bin'
damage "版本号改成 2" 'python3 -c "b=bytearray(open(\"index.good.bin\",\"rb\").read()); b[8]=2; open(\"index.bin\",\"wb\").write(bytes(b))"'
damage "垃圾文件" 'printf "not an index at all" > index.bin'
cp index.good.bin index.bin

echo "== 11. 边界：空文档 / 空索引"
mkdir -p empty && cd empty || exit 1
: > empty.txt
if "$WORK/index_gen" --theta=0.5 empty.txt 2>empty.log; then
  # 一个文件 = 一篇文档，所以空文件也是 1 篇文档（N=1），只是没有任何位置条目；
  # index.bin 只有头部 64 B + docs 段的 4 B。
  if grep -q 'N=1 documents' empty.log && [ "$(stat -c%s index.bin)" = 68 ]; then
    ok "空文档 -> N=1、索引只有头部 + docs 段（68 字节）"
  else
    bad "空文档的结果不符合预期"; cat empty.log
  fi
  if "$WORK/query" the >/dev/null 2>&1 && grep -q '^Not found: the' output.txt; then
    ok "空索引下查询正常（Not found）"
  else
    bad "空索引下查询异常"
  fi
else
  bad "空文档时 index_gen 失败"; cat empty.log
fi
# N=0 的空索引仍必须能加载（load 侧的 ndocs == 0 分支）
python3 - <<'PYEOF'
import struct
head = b'PR1IDX\x00\x00' + struct.pack('<II', 1, 64) + struct.pack('<QQQ', 0, 0, 0) + struct.pack('<QQQ', 64, 64, 64)
assert len(head) == 64
open('index.bin', 'wb').write(head)
PYEOF
if "$WORK/query" the >/dev/null 2>&1 && grep -q '^Not found: the' output.txt; then
  ok "N=0 的 64 字节空索引仍能加载并查询"
else
  bad "N=0 空索引加载失败"
fi
cd "$WORK" || exit 1

echo
echo "== 结果: PASS=$pass FAIL=$fail    （工作目录 $WORK 保留，便于人工查看）"
[ "$fail" -eq 0 ]
