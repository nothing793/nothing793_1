#!/usr/bin/env python3
"""生成确定性合成语料，供 pr1 的整条流水线在没有真实语料时跑通与验证。

用法：
    python3 gen_corpus.py OUTDIR [--docs 40] [--words 5000] [--seed 1]

设计要点：
  * 每篇文档混三类词 —— 高频功能词（the / of / and ...，会跨文档大量出现，正是
    Part 1 要识别的 stop words）、中等频度的普通词（run / index / king ...）、
    以及少量伪词（唯一出现几次的"稀有词"，用于观察 df/N 小的词）；
  * 带大小写、标点、撇号、连字符与数字，让切词口径真的被用到；
  * 往两篇文档里塞入固定句子 "To be, or not to be: that is the question."，
    用来对拍短语查询与位置编号；
  * 随机数固定种子 → 同样的参数永远生成同样的语料（可复现、可 diff）。
"""
import argparse
import bisect
import itertools
import os
import random

FUNCTION_WORDS = """the of and to a in that is was he for it with as his on be at by i this had not are
but from or have an they which one you were her all she there would their we him been has when who will
more no if out so said what up its about into than them can only other new some could time these two may
then do first any my now such like our over man me even most made after also did many before must""".split()

COMMON_WORDS = """run walk query index position stem word document search engine threshold noisy interesting
shakespeare play act scene king queen lord soldier love death night day sun moon sea ship sword crown ghost
forest castle garden letter horse battle friend mother father brother sister daughter son""".split()

SYLLABLES = "ka mo ri ta ne so lu va we zi du pa qo he xy ju fy gy vy by cy dy".split()

PUNCTUATION = [",", ".", ";", ":", "!", "?", "'s", "-", ")", "("]

# 两段固定句子（插进第 0、3 篇文档）：前一段全是功能词，用来演示"短语里含停用词就查不到"；
# 后一段全是实词，用来对拍短语查询的位置编号。
MARKER = "To be, or not to be: that is the question."
CONTENT_MARKER = "zolkim vireth qandel brusett nomin"


def pseudo_word(rng):
    return "".join(rng.choice(SYLLABLES) for _ in range(rng.randint(2, 3)))


def build_vocab(rng, size):
    """按 Zipf 权重排好的词表：排名越靠前越常出现。
    词表要足够大，否则每篇文档都会覆盖到几乎全部词，df/N 会集体超过 θ，
    整份词表都被判成停用词（那样就测不出别的了）。"""
    vocab = list(COMMON_WORDS)
    seen = set(vocab)
    guard = 0
    while len(vocab) < size and guard < size * 20:
        word = pseudo_word(rng)
        guard += 1
        if word not in seen:
            seen.add(word)
            vocab.append(word)
    return vocab


def make_sampler(rng, vocab, weights):
    """预先把 Zipf 权重变成累积表 + 二分采样：比 rng.choices(..., weights=...) 快两个数量级。"""
    cumulative = list(itertools.accumulate(weights))

    def sample():
        return vocab[bisect.bisect(cumulative, rng.random() * cumulative[-1])]
    return sample


def make_doc(rng, vocab, weights, word_count, with_marker):
    sample = make_sampler(rng, vocab, weights)
    out = []
    if with_marker:
        out.append(MARKER)
        out.append(CONTENT_MARKER)
    for _ in range(word_count):
        roll = rng.random()
        if roll < 0.45:
            word = rng.choice(FUNCTION_WORDS)
        else:
            word = sample()
        if rng.random() < 0.08:
            word = word.capitalize()
        out.append(word)
        if rng.random() < 0.12:
            out[-1] += rng.choice(PUNCTUATION)
    text = " ".join(out)
    # 折成每行 ~12 个词，顺便让"跨行"的词真的出现（换行只是分隔符）
    lines = text.split(" ")
    rows = [" ".join(lines[i:i + 12]) for i in range(0, len(lines), 12)]
    return "\n".join(rows) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--docs", type=int, default=40)
    ap.add_argument("--words", type=int, default=5000)
    ap.add_argument("--vocab", type=int, default=8000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    vocab = build_vocab(rng, args.vocab)
    weights = [1.0 / (rank + 1) for rank in range(len(vocab))]

    os.makedirs(args.outdir, exist_ok=True)
    total = 0
    for doc in range(args.docs):
        # 每篇文档有自己的"主题词"：把它们放在词表前部，使它们在中频段出现
        doc_vocab = vocab
        text = make_doc(rng, doc_vocab, weights, args.words, with_marker=(doc in (0, 3)))
        path = os.path.join(args.outdir, "doc_%03d.txt" % doc)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
        total += args.words

    print("wrote %d documents (%d words) to %s (seed=%d)" % (args.docs, total, args.outdir, args.seed))


if __name__ == "__main__":
    main()
