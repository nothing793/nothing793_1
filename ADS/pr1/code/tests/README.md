# pr1 测试目录

本目录是 pr1 迷你搜索引擎的测试与实验脚本，对应的正文说明见
[`../README.md`](../README.md) 第 12 节。

**这里没有构建产物**：所有编译和执行都在临时工作目录里进行（默认 `/tmp/pr1-test`，
可以用 `run_tests.sh <工作目录>` 换），不会污染 `code/`。

## 文件

| 文件 | 用途 |
| --- | --- |
| `run_tests.sh` | 端到端测试入口：编译（零告警）→ 生成语料 → 切词（`index_gen --dump-tokens`）→ 建索引 → 各项校验，逐项打印 PASS/FAIL（当前 21 项全过） |
| `gen_corpus.py` | 确定性合成语料生成器（没下载莎士比亚全集前用它跑通整条流水线）。功能词比例可调，保证一定会出现"跨文档高频词"这类停用词 |
| `stem_list.c` | 词干命令行工具：从 stdin 读词、逐行输出词干。只给 `brute_check.py` 用（Python 侧没有 Porter 实现） |
| `pick_term.py` | 从 `file.txt` 里挑一个词干并打印它的 df / N（可按 df/N 区间、最高频、最稀有），供 `sweep.py` 与阈值测试动态选词 |
| `brute_check.py` | 独立校验：用另一份实现解析 `index.bin`，与从 `file.txt` 暴力统计的结果逐词条对比（含 df / tf / 位置链、是否剔除了停用词）；另外可以暴力枚举短语出现位置，用来对拍短语查询 |
| `roundtrip.c` | 读 `index.bin` → 用 `index_save()` 再写一份，供 `cmp` 验证"加载再落盘逐字节相同"（编译时链 `../stem.c`，索引实现在 `../index.h`） |
| `sweep.py` | 第 4 问的阈值实验：θ ∈ {0.3,0.4,0.5,0.6} × τ ∈ {0.1,0.2,0.3,0.4}，打印 Markdown 表格（stoplist 大小、平均结果集大小、被阈值拦下的比例） |

## 用法

```bash
sh run_tests.sh                      # 全套测试，默认工作目录 /tmp/pr1-test
python3 sweep.py /tmp/pr1-test       # 阈值敏感性实验（先用 run_tests.sh 准备好工作目录）
```

`run_tests.sh` 依赖 `python3`、`gcc`、`tr`、`diff`、`cmp`。语料是合成的：
换成真实的莎士比亚全集，只要把命令行里的语料换成莎士比亚的文本即可，
其余步骤（θ / τ 实验、对拍）完全一样。
