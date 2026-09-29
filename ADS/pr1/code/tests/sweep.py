#!/usr/bin/env python3
"""题面第 4 问：阈值对结果的影响。打印两张 Markdown 表。

用法：
    python3 sweep.py [工作目录]          # 默认 /tmp/pr1-test（先跑一次 run_tests.sh）

表 A：θ（建索引时的停用词阈值）→ stoplist 大小、索引词条数、索引体积、保留的位置比例；
表 B：τ（查询时的 df/N 阈值）→ 三个查询（高频 / 中频 / 极稀有词）的结果集大小，
      以及被硬阈值拦下的查询比例。
"""
import os
import re
import subprocess
import sys

THETAS = [0.3, 0.4, 0.5, 0.6]
TAUS = [None, 0.1, 0.2, 0.3, 0.4]
THETA_FOR_TAU = 0.5          # 表 B 固定用这个 θ 建索引
TESTS = os.path.dirname(os.path.abspath(__file__))


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def pick_terms(work):
    """挑三个"索引里确实存在"的查询词：高频（df/N 接近 θ）、中频、极稀有（df = 1）。
    注意不能拿语料里最高频的词来查 —— 那些词已经按 Part 1 从索引里剔除了，
    查它们只会得到 "Not found"，测不出 τ 的作用。"""
    def pick(*extra):
        return run(["python3", os.path.join(TESTS, "pick_term.py"), "--tokens", "file.txt",
                    "--stemmer", "./stem_list"] + list(extra), work).stdout.split()

    common = pick("--min", "0.35", "--max", "0.45") or pick("--min", "0.2", "--max", "0.45")
    mid = pick("--min", "0.10", "--max", "0.20") or pick("--min", "0.05", "--max", "0.3")
    rare = pick("--rarest")
    return [("高频", common[0], common[3]), ("中频", mid[0], mid[3]), ("极稀有", rare[0], rare[3])]


def build_index(work, theta):
    proc = run(["./index_gen", "--theta=%.2f" % theta], work)
    stop_words = 0
    if os.path.exists(os.path.join(work, "stoplist.txt")):
        with open(os.path.join(work, "stoplist.txt"), encoding="utf-8") as fh:
            stop_words = sum(1 for line in fh if line.strip() and not line.startswith("#"))
    terms = 0
    match = re.search(r"wrote (\d+) terms", proc.stderr)
    if match:
        terms = int(match.group(1))
    kept = re.search(r"pass 2 \(index generation\): (\d+) positions", proc.stderr)
    total = re.search(r"pass 1 \(statistics\): (\d+) positions", proc.stderr)
    ratio = ""
    if kept and total and int(total.group(1)):
        ratio = "%.1f%%" % (100.0 * int(kept.group(1)) / int(total.group(1)))
    size = os.path.getsize(os.path.join(work, "index.bin"))
    return stop_words, terms, size, ratio


def query_size(work, word, tau):
    cmd = ["./query"]
    if tau is not None:
        cmd.append("--tau=%.2f" % tau)
    cmd.append(word)
    run(cmd, work)
    blocked = False
    hits = 0
    with open(os.path.join(work, "output.txt"), encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("Too common:"):
                blocked = True
            elif re.match(r"^Doc ID: \d+, Position: ", line):
                hits += 1
    return hits, blocked


def main():
    work = sys.argv[1] if len(sys.argv) > 1 else "/tmp/pr1-test"
    terms = pick_terms(work)

    print("### 表 A：θ（Part 1/2 的停用词阈值，N=40 篇文档）\n")
    print("| θ | stoplist 条数 | 索引词条数 | index.bin | 保留的位置条目 |")
    print("| --- | --- | --- | --- | --- |")
    for theta in THETAS:
        stop_words, term_count, size, ratio = build_index(work, theta)
        print("| %.1f | %d | %d | %.1f KB | %s |" % (theta, stop_words, term_count, size / 1024.0, ratio))

    build_index(work, THETA_FOR_TAU)
    print("\n### 表 B：τ（查询时的 df/N 阈值，索引用 θ = %.1f 建）\n" % THETA_FOR_TAU)
    print("查询词：" + "、".join("%s(%s, df/N=%s)" % (name, stem, ratio) for name, stem, ratio in terms))
    print()
    print("（τ 只有落在 (0, θ] 里才有效：df/N > θ 的词在 Part 2 就已经不在索引里了，"
          "查询侧只会报 Not found。）\n")
    print("| τ | %s | %s | %s | 被拦下的查询 |" % tuple(name for name, _, _ in terms))
    print("| --- | --- | --- | --- | --- |")
    for tau in TAUS:
        cells = []
        blocked_count = 0
        for _name, stem, _ratio in terms:
            hits, blocked = query_size(work, stem, tau)
            if blocked:
                cells.append("拦下")
                blocked_count += 1
            else:
                cells.append("%d 篇" % hits)
        label = "不设" if tau is None else "%.1f" % tau
        print("| %s | %s | %d/%d |" % (label, " | ".join(cells), blocked_count, len(terms)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
