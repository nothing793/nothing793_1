#!/usr/bin/env python3
"""独立校验 index.bin 的内容（另一份实现：本文件按 index.h 的格式规范重新解析）。

用法：
    python3 brute_check.py --index index.bin --tokens file.txt --stemmer ./stem_list
                          [--stoplist stoplist.txt] [--phrase "to be or not to be"]

校验内容：
  1. 结构：头部魔数/版本、段偏移递增、terms 段长度、所有词条的 tf 之和 == 头部位置总数、
     postings 段正好读完、文件末尾没有多余字节；
  2. 内容：把 file.txt 里每个词词干化后暴力聚合，逐词条与索引里的 (df, tf, 位置链) 完全相等；
     给了 --stoplist 就要求索引里**恰好**是"全部词条 - 停用词"（即 Part 2 真的剔除了停用词）；
  3. （可选）--phrase：暴力枚举该短语在 file.txt 里出现的 (doc, start)，打印成
     "doc start" 行，供 shell 侧与 query 的输出对拍。
"""
import argparse
import subprocess
import sys
from collections import defaultdict

MAGIC = b"PR1IDX\x00\x00"


def read_u32(fh):
    return int.from_bytes(fh.read(4), "little")


def read_u64(fh):
    return int.from_bytes(fh.read(8), "little")


def read_u16(fh):
    return int.from_bytes(fh.read(2), "little")


def read_varint(fh):
    result = 0
    shift = 0
    while True:
        raw = fh.read(1)
        if not raw:
            raise ValueError("unexpected EOF inside a varint")
        byte = raw[0]
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return result
        shift += 7


def parse_index(path):
    with open(path, "rb") as fh:
        blob = fh.read()
    if blob[:8] != MAGIC:
        raise ValueError("bad magic")
    version = int.from_bytes(blob[8:12], "little")
    header_size = int.from_bytes(blob[12:16], "little")
    n_docs = int.from_bytes(blob[16:24], "little")
    n_terms = int.from_bytes(blob[24:32], "little")
    n_positions = int.from_bytes(blob[32:40], "little")
    off_docs = int.from_bytes(blob[40:48], "little")
    off_terms = int.from_bytes(blob[48:56], "little")
    off_postings = int.from_bytes(blob[56:64], "little")
    assert version == 1, version
    assert header_size == 64, header_size
    assert off_docs == header_size and off_docs < off_terms <= off_postings, (
        off_docs, off_terms, off_postings)
    assert off_docs + 4 * n_docs == off_terms, (off_docs, n_docs, off_terms)

    with open(path, "rb") as fh:
        fh.seek(0)
        fh.read(header_size)
        docs = [read_u32(fh) for _ in range(n_docs)]
        terms = []
        for _ in range(n_terms):
            df = read_u32(fh)
            tf = read_u32(fh)
            offset = read_u64(fh)
            length = read_u16(fh)
            stem = fh.read(length).decode("utf-8")
            terms.append((stem, df, tf, offset))
        if fh.tell() != off_postings:
            raise ValueError("terms section length mismatch: %d != %d" % (fh.tell(), off_postings))

        postings = {}
        total_positions = 0
        for stem, df, tf, offset in terms:
            if fh.tell() != offset:
                raise ValueError("posting offset mismatch for %r" % stem)
            positions = []
            doc = 0
            groups = 0
            counted = 0
            while groups < df:
                doc += read_varint(fh)   # 第一组的增量相对 0 算
                count = read_varint(fh)
                pos = -1
                for _ in range(count):
                    pos += read_varint(fh)
                    positions.append((doc, pos))
                counted += count
                groups += 1
            if groups != df or counted != tf:
                raise ValueError("df/tf mismatch for %r" % stem)
            total_positions += counted
            postings[stem] = positions
        trailing = len(blob) - fh.tell()
        if trailing != 0:
            raise ValueError("%d trailing bytes after the postings section" % trailing)
    if total_positions != n_positions:
        raise ValueError("header position count %d != decoded %d" % (n_positions, total_positions))
    if sorted(docs) != docs or len(set(docs)) != len(docs):
        raise ValueError("docs section is not ascending/unique")
    return docs, postings


def stem_words(stemmer, words):
    out = subprocess.run([stemmer], input="\n".join(words), capture_output=True, text=True, check=True)
    # 末尾那个换行会 split 出一个空串，必须去掉，否则短语窗口长度会多 1
    return [line for line in out.stdout.split("\n") if line != ""]


def brute_force(tokens_path, stemmer, stoplist_path):
    rows = []
    with open(tokens_path, "r", encoding="utf-8") as fh:
        for line in fh:
            fields = line.split()
            if len(fields) != 3:
                continue
            word, doc, pos = fields[0], int(fields[1]), int(fields[2])
            rows.append((word, doc, pos))

    stems = stem_words(stemmer, [row[0] for row in rows])
    skipped = set()
    if stoplist_path:
        with open(stoplist_path, "r", encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("#"):
                    continue
                fields = line.split()
                if fields:
                    skipped.add(fields[0])

    grouped = defaultdict(set)
    for (word, doc, pos), stem in zip(rows, stems):
        grouped[stem].add((doc, pos))
    expected = {stem: sorted(pos) for stem, pos in grouped.items() if stem not in skipped}
    all_stems = set(grouped)
    return expected, all_stems, skipped, rows, stems


def brute_phrase(rows, stems, phrase_words, stemmer):
    wanted = stem_words(stemmer, phrase_words)
    per_doc = defaultdict(list)
    for (word, doc, pos), stem in zip(rows, stems):
        per_doc[doc].append((pos, stem))
    hits = []
    for doc, entries in per_doc.items():
        entries.sort()
        for i in range(len(entries) - len(wanted) + 1):
            window = entries[i:i + len(wanted)]
            if [stem for _, stem in window] != wanted:
                continue
            if all(window[k][0] == window[0][0] + k for k in range(len(wanted))):
                hits.append((doc, window[0][0]))
    return sorted(hits)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", required=True)
    ap.add_argument("--tokens", required=True)
    ap.add_argument("--stemmer", required=True)
    ap.add_argument("--stoplist")
    ap.add_argument("--phrase")
    args = ap.parse_args()

    docs, postings = parse_index(args.index)
    expected, all_stems, skipped, rows, stems = brute_force(args.tokens, args.stemmer, args.stoplist)

    problems = []
    if len(docs) != len({doc for _, doc, _ in rows}):
        problems.append("document count %d != %d in file.txt" % (len(docs), len({d for _, d, _ in rows})))
    if set(postings) != set(expected):
        missing = sorted(set(expected) - set(postings))[:5]
        extra = sorted(set(postings) - set(expected))[:5]
        problems.append("term set differs (missing=%s extra=%s)" % (missing, extra))
    else:
        for stem in expected:
            if postings[stem] != expected[stem]:
                problems.append("positions differ for %r" % stem)
                break
    if args.stoplist:
        leaked = sorted(skipped & set(postings))[:5]
        if leaked:
            problems.append("stop words still in the index: %s" % leaked)
        if all_stems - skipped != set(expected):
            problems.append("brute force/stoplist bookkeeping mismatch")

    if problems:
        print("FAIL: " + "; ".join(problems))
        return 1
    print("PASS: %d terms, %d positions, %d documents (index == file.txt minus %d stop words)"
          % (len(postings), sum(len(v) for v in postings.values()), len(docs), len(skipped)))

    if args.phrase:
        hits = brute_phrase(rows, stems, args.phrase.split(), args.stemmer)
        print("PHRASE %s: %d hit(s)" % (args.phrase, len(hits)))
        for doc, start in hits:
            print("%d %d" % (doc, start))
    return 0


if __name__ == "__main__":
    sys.exit(main())
