#!/usr/bin/env python3
"""从 file.txt 里挑一个词干，打印 "<stem> <df> <N> <df/N>"，用于 τ 实验与阈值测试。

用法：
    python3 pick_term.py --tokens file.txt --stemmer ./stem_list [--min 0.15] [--max 0.45]
    python3 pick_term.py --tokens file.txt --stemmer ./stem_list --most-common
    python3 pick_term.py --tokens file.txt --stemmer ./stem_list --rarest
"""
import argparse
import collections
import subprocess


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tokens", required=True)
    ap.add_argument("--stemmer", required=True)
    ap.add_argument("--min", type=float, default=0.0)
    ap.add_argument("--max", type=float, default=1.0)
    ap.add_argument("--most-common", action="store_true")
    ap.add_argument("--rarest", action="store_true")
    args = ap.parse_args()

    rows = []
    with open(args.tokens, encoding="utf-8") as fh:
        for line in fh:
            fields = line.split()
            if len(fields) == 3:
                rows.append((fields[0], fields[1]))
    out = subprocess.run([args.stemmer], input="\n".join(r[0] for r in rows),
                         capture_output=True, text=True, check=True)
    stems = out.stdout.split("\n")

    df = collections.defaultdict(set)
    for (_word, doc), stem in zip(rows, stems):
        df[stem].add(doc)
    total = len({doc for _, doc in rows})
    if total == 0:
        return 1

    ranked = sorted(((len(docs), stem) for stem, docs in df.items()), reverse=True)
    if args.most_common:
        picked = ranked[0]
    elif args.rarest:
        picked = ranked[-1]
    else:
        band = [item for item in ranked if args.min < item[0] / total <= args.max]
        if not band:
            return 1
        picked = band[0]

    count, stem = picked
    print("%s %d %d %.4f" % (stem, count, total, count / total))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
