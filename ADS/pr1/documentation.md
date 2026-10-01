# pr1 完整报告：自己写一个迷你搜索引擎

> 课程：ADS（算法与数据结构）；题目：**8-1 Roll Your Own Mini Search Engine**（题面截图见 `image.png`）。
> 代码在 [`code/`](code/)，设计细节与踩坑记录见 [`code/README.md`](code/README.md)，
> 使用说明见 [`readme.md`](readme.md)。本文是完整报告：题目分析、方法、实验与验证。
>
> **语料说明（重要）**：本报告的实验在**确定性合成语料**上完成（40 篇 × 4000 词，
> `code/tests/gen_corpus.py` 生成，固定随机种子）。真实语料（莎士比亚全集，
> `shakespeare.mit.edu`）按指示**尚未下载**；换成真实语料只需
> `./index_gen <每部剧一个文件>`，之后的查询与全部阈值实验可原样复用，
> 结论（θ / τ 的单调性与有效区间）与语料无关。

## 1. 题目要求

1. 对莎士比亚全集做**词频统计**，识别 stop words（noisy words），并回答
   *"How and where do you draw the line between 'interesting' and 'noisy' words?"*（1 pt.）
2. 建立**带词干化**的倒排索引，且 *The stop words identified in part (1) must not be included*（5 pts.）
3. 写**查询程序**，在倒排文件索引之上接受用户给出的词（或短语），返回包含它的文档 ID（3 pts.）
4. **测试阈值对查询结果的影响**（Testing 2 pts.）
5. Bonus：*What if you have 500 000 files and 400 000 000 distinct words?*（额外加分）

本实现把题目拆成三段流水线 + 两个实验脚本：

```
原始文本 ─┬─ index_gen pass 1 ─> stoplist.txt（部分 1 的统计）
          └─ index_gen pass 2 ─> index.bin ── query ──> output.txt
```

| 题面 | 实现位置 | 产物 |
| --- | --- | --- |
| (1) 词频 / 停用词 | `code/index_gen.c` pass 1（`collect_stats()` / `stoplist_build()` / `wc_report()`） | `stoplist.txt`、`--count-only` 报告 |
| (2) 倒排索引 | `code/index_gen.c` pass 2（`tokenize_document()` + `index_save()`） | `index.bin` |
| (3) 查询 | `code/query.c`（单词 / 多词 AND / 短语） | `output.txt` |
| (4) 阈值实验 | `code/query.c --tau=` + `code/tests/sweep.py` | 第 6 节的表 B |

## 2. 总体设计

### 2.1 为什么是"两遍扫描"

`df`（出现某词干的文档数）是**整份语料**的统计量，`N` 只有扫完才知道；而题面明写索引里**不能包含**
Part 1 认定的停用词。所以必须在建索引之前先完成统计：

```
pass 1：读原始语料 → 建一份内存索引（只为拿 cf / df）→ df/N > θ 的词写进 stoplist.txt
pass 2：丢掉 pass 1 的索引 → 再读一遍语料 → 跳过 stoplist 里的词 → 建索引 → index.bin
```

两遍都调用同一个 `tokenize_document()` 与 `index_add_position()`，因此切词、词干化、去重、排序的口径
不可能出现"统计一套、建索引另一套"。代价是时间约翻倍（合成语料 0.79 s，真实语料 88 万条三元组约 2.6 s），
换来的是"索引里有没有停用词"这件事可验证（第 7 节的对拍就是验证这一点）。

### 2.2 为什么把词频统计并入建索引程序

按题面字面（*the programs for word counting, index generation and query processing*）可以把 Part 1
做成独立可执行文件、把 stoplist 写成中间文件再喂给 Part 2。这样做有四个实际代价：

1. 多一份 `stoplist.txt` 的格式规范、加载器与错误处理；
2. **陈旧输入会静默出错**：`stoplist.txt` 与语料不同步时，`index.bin` 结构完全合法、
   加载成功、查询正常，只是内容错了 —— 这比格式损坏更难发现；
3. 可复现性退化：原来是"同一份语料跑两次得到逐字节相同的 `index.bin`"，
   外挂 stoplist 之后变成"语料 + `stoplist.txt` + θ 三者都相同"；
4. 两处各自的切词 / 词干化口径必须人工保持一致，而**口径不一致是这类作业最常见的隐蔽 bug**。

合并后，`stoplist.txt` 只作为 Part 1 的**证据与解释材料**（`query` 用它把 `Not found` 说明成
"这是 Part 1 剔除的停用词"），不参与查询正确性。同时用 `./index_gen --count-only` 保留
"只做 Part 1、不建索引"的独立入口，便于演示与对拍。

### 2.3 为什么切词层不做词干化

`stemword()`（转小写 + Porter）全流程**只能做两次**：建索引时一次、查询时一次。
如果切词层先词干化、建索引时又切一次，而 **Porter 不是幂等的**。
用本项目的 `stem.c` 实测 604 个常见词：

```
tested=604 non-idempotent=11
because -> becaus -> becau      release -> releas -> relea
license -> licens -> licen      increases -> increas -> increa
equivalent -> equival -> equiv  ...
```

后果是索引里存 `becau`、查询侧算出 `becaus`：**查不到，而且 `index.bin` 从里到外完全合法**。
因此 `code/index_gen.c` 的切词段明确规定：切词层只做 `isalnum` 连续段切分 + `tolower`，
词干化统一交给 `stemword()`；入库的是词干，`--dump-tokens` 写出的则是**转小写后的原形**。

## 3. Part 1：词频统计与停用词识别

### 3.1 "noisy 与 interesting 的线画在哪"

题面这句话本身就是要求：必须给出**可复现的量化准则**并讨论它的代价。三种候选：

| 准则 | 定义 | 优点 | 代价 |
| --- | --- | --- | --- |
| A. 固定表 | 经典 ~300 个英文功能词 | 简单、可复现、与语料无关 | 语料相关噪声漏掉（莎士比亚里的 `thou` / `thee` / `hath` / `doth` 不在表里） |
| B. 全局频次 cf | `cf / 总词次 > θ` | 不需要文档粒度统计 | 误杀"高频但有区分度"的词（某部剧里反复出现的角色名） |
| **C. 文档频率 df** | `df / N > θ`，df = **含该词的文档数**（本实现） | 它衡量的正是"这个词能不能区分文档"，是 IR 的标准做法 | 需要一次全语料统计（因此要两遍扫描） |

本实现取 **C**：`df/N` 接近 1 的词几乎出现在每一篇文档里，对"哪些文档含它"没有信息量，就是噪声；
反过来，只在少数文档里出现的词才"能区分文档"。`df` 的统计复用建索引的插入入口，
所以 `df/N` 的分子分母与索引里的 `Posting` 完全同源。

### 3.2 产物

```console
$ ./index_gen --count-only | head -12
== Part 1: word count ==
documents (N)      : 40
distinct stems (V) : 7124
position entries   : 161892
theta              : 0.500  (df/N > theta -> stop word)

== stop words: 422 / 7124 (5.9%) ==
stem                         cf         df     df/N
run                        9241         40   1.0000
walk                       4509         40   1.0000
...
```

`stoplist.txt` 的表头同时记录 θ、N、V 与停用词条数，方便报告与复现（`query` 会读它做提示）。

### 3.3 θ 的影响（实验）

| θ | stoplist 条数 | 索引词条数 | index.bin | 保留的位置条目 |
| --- | --- | --- | --- | --- |
| 0.3 | 733 | 6391 | 235.2 KB | 14.2% |
| 0.4 | 532 | 6592 | 252.2 KB | 16.4% |
| **0.5（默认）** | **422** | **6702** | **263.8 KB** | **18.1%** |
| 0.6 | 340 | 6784 | 274.3 KB | 19.7% |

θ 越小 ⇒ 判为噪声的词越多 ⇒ 索引越小、查询越快，但 82% 的正文位置条目被丢弃（θ=0.5 时），
**含停用词的短语会彻底查不到**（`"to be or not to be"` 的五个词全在 stoplist 里 → 0 命中）。
这是 Part 2 的必然副作用，取舍见 6.3 与第 8 节。

## 4. Part 2：带词干化的倒排索引

### 4.1 内存结构

`djb2` 哈希（`hashsize` = 1000007 个桶）+ 链地址法；每个词干一个 `Posting`，
挂一条按 `(doc_id, pos)` **升序**的 `Position` 链 —— `df`、求交、短语验证都依赖这个有序性。
去重键是 `(词干, doc_id, pos)`。`sizeof(inverted_index) ≈ 7.6 MB`（桶数组嵌在结构体里），
所以**必须堆分配**，放栈上会直接段错误。

### 4.2 文件格式（`index.bin` v1）

```
头部 64 B：魔数 "PR1IDX"+两个 '\0'、u32 版本(=1)、u32 头部长度(=64)、
           u64 文档数、u64 词条数、u64 位置条目总数、u64 docs/terms/postings 三段偏移
docs 段：     文档数 × u32（doc_id 升序去重，给 df/N 提供 N）
terms 段：    词条数 × { u32 df、u32 tf、u64 postings 偏移、u16 词干长度、词干字节 }，按词干升序
postings 段： [doc 增量][该文档的位置数][位置增量 × 位置数] 重复 df 次，全部 LEB128
```

设计取舍：

- **小端 + 定长、不 dump 结构体**：字节序、`int` 宽度、padding 都进不了文件；
- **gap + LEB128**：相邻命中文档、同一文档内相邻位置的平均间隔都是 1 字节量级，这是"为什么用二进制"的答案；
- **词条按词干升序**：输出与哈希桶顺序无关 ⇒ 同一份 `file.txt` 永远得到逐字节相同的 `index.bin`
  （可复现、可 diff，也是往返测试 `cmp` 的基础）；
- **`df` / `tf` 冗余存进词典**：既省掉查询侧重算，又能在加载时交叉校验。

### 4.3 完整性

`index_load()` 会校验魔数、版本、头部长度、段偏移递增、各段不越界、词条数不超过
"terms 段字节数 / 18"（防住被改坏的头部导致天文数字内存分配）、每个词条的 df / tf 与解出的
组数 / 位置数一致、所有词条合计等于头部的位置总数、文件末尾无多余字节 —— 任何一项不符都返回 NULL。
写侧则先写 `index.bin.tmp` 再 `rename()`（同目录原子替换），并检查 `index_save()` 返回值、
`fclose()` 返回值与 `ferror()`，失败就删掉 `.tmp` 并报错退出：**绝不留下半截索引**。
已知局限：v1 没有校验和，改坏一两个字节但结构仍自洽的文件会被接受（根治办法是把头部扩到
72/80 字节加校验和字段并递增版本号，留作 v2）。

## 5. Part 3：查询程序

`./query` 加载 `index.bin` 后支持三种查询：

| 调用 | 语义 | 实现 |
| --- | --- | --- |
| `./query wevafi` | 单词 | 词干化后查表，打印 `df/N`、`tf` 与全部 `(doc, pos)` |
| `./query wevafi zebra` | 多词 AND | 对各词的文档集合（升序数组）做 k 路求交 |
| `./query "a b c"` | 短语 | 先求交得到候选文档，再用位置链验证 `pos, pos+1, pos+2...` 连续 |

要点：

- 求交时 **`k` 路指针每轮都严格前进**（锚点跳到一个不可能命中的位置之后要重新推进），否则会死循环；
- 短语验证用 **tf 最小的词当锚点**：它的候选起始位置最少，验证代价最低；
- 位置链本来就是按 `(doc, pos)` 升序的，所以"某文档内某词的位置数组"可以直接顺序取出，
  连续性是**二分查找**判断的（`contains_int`）。

输出示例（`output.txt`）：

```
# query 1: zolkim vireth qandel brusett nomin
Found: zolkim  (df=2/40 = 0.050, tf=2)
...
Phrase [zolkim vireth qandel brusett nomin]: 2 match(es) in 2 document(s)
Doc ID: 0, Positions: 10
Doc ID: 3, Positions: 10

# query 2: be
Not found: be  (stop word: df=40/40 = 1.0000 > theta=0.5000, removed from the index in part 1)
```

## 6. Part 4：阈值实验

### 6.1 τ 的定义与行为

`df/N > τ` 的词判为 `too common`，走**硬阈值**：

```
Too common: wevafi  (df=18/40 = 0.450, tf=27, tau=0.200 -> 18 documents, result list suppressed)
```

即：报出命中文档数（这样"结果到底有多大"是可解释的），**但不打印文档列表**；
在多词查询里该词被排除出交集（于是阈值对最终结果集的影响直接可见）。默认不设 τ。

### 6.2 实验结果（θ = 0.5，N = 40）

三个查询词分别取"索引里确实存在"的高频 / 中频 / 极稀有词：

| τ | 高频词 df/N=0.450 | 中频词 df/N=0.200 | 极稀有词 df/N=0.025 | 被拦下的查询 |
| --- | --- | --- | --- | --- |
| 不设 | 27 篇 | 11 篇 | 1 篇 | 0/3 |
| 0.1 | 拦下 | 拦下 | 1 篇 | 2/3 |
| 0.2 | 拦下 | 11 篇 | 1 篇 | 1/3 |
| 0.3 | 拦下 | 11 篇 | 1 篇 | 1/3 |
| 0.4 | 拦下 | 11 篇 | 1 篇 | 1/3 |

### 6.3 分析

1. **τ 的单调性**：τ 越小，被拦下的查询越多、平均结果集越小 —— 阈值就是把"结果集大到没有意义"
   的查询挡在前面。
2. **τ 的有效区间被 θ 限制**：`df/N > θ` 的词在 Part 2 就已经不在索引里了，查询侧根本看不到它们的
   `df`，只会得到 `Not found`（甚至被提示成停用词）。所以真正可调的区间是 `(0, θ]`：
   上表里 τ = 0.4 与 τ = 0.3 完全一样，因为 0.45 的高频词在 θ = 0.5 的索引里已经是"最高一档"。
   **两个阈值必须一起讨论**，这也是"Part 4 要跑测试"而不是"拍一个数"的原因。
3. **硬阈值的代价与替代**：硬阈值让提示可解释，但代价是**完全丢掉**该词的结果列表；
   真实系统应当用软阈值 —— 返回 top-K 并按 tf-idf / BM25 排序（见第 9 节 Bonus 的 WAND / MaxScore）。
4. **停用词与短语的冲突**：θ 越大（剔得越狠），短语查询的可用性越差。若要支持"含停用词的短语"，
   要么对短语查询单独保留位置索引、要么把 stoplist 分成"索引期剔除"与"查询期过滤"两档。
   本实现选择了诚实的做法：明确告诉用户这个词是 Part 1 剔除的停用词，而不是笼统的 `Not found`。

## 7. 正确性验证

测试入口 `code/tests/run_tests.sh`：在临时目录里编译、生成合成语料、跑完整流水线，
当前 **21 项全部 PASS**（语料：40 篇 / 161,892 条三元组 / N=40 / V=7,124 / θ=0.5）。

| 检查项 | 方法 | 结果 |
| --- | --- | --- |
| 编译 | 四个程序在 `-std=c99 -Wall -Wextra -Wpedantic` 下编译（交付源文件只有 `index_gen.c` / `query.c`） | 零告警 |
| 切词口径 | `index_gen --dump-tokens` 的词表 vs `tr -cs 'A-Za-z0-9' '\n' \| tr A-Z a-z`；另对拍 stdin 输入 | 逐词一致（4,057 词） |
| **索引内容（独立对拍）** | `tests/brute_check.py` 另按 `index.h` 规范（含 LEB128 解码）重写解析器，与"从 `file.txt` 暴力聚合"逐词条比较 | 词条集合 / df / tf / 完整位置链全等：6702 terms, 29296 positions |
| **停用词确实未入索引** | 同上，要求索引 == 全部词条 − stoplist | 通过 |
| 结构自洽 | 魔数 / 版本 / 段偏移递增 / terms 段长度 / 位置总数 / 文件末尾无多余字节 | 通过 |
| **编解码往返** | `tests/roundtrip.c`：load → save → `cmp` | 逐字节相同 |
| 短语查询 | 暴力枚举短语的 (doc, start) vs `query` 输出 | 一致（doc 0/3，pos 10） |
| 损坏文件 | 截断 1 字节 / 版本号改 2 / 纯垃圾 | 全部被拒绝，`query` 非零退出 |
| 边界 | 空文档（N=1、68 字节索引）可加载查询；手工构造的 N=0 空索引（64 字节）也能加载 | 通过 |
| 阈值 τ | df/N 在 (0.15, 0.45] 的词在 τ=0.2 时必须被拦下 | 通过 |

**已知未覆盖**：v1 无校验和（改坏一字节但结构自洽的文件会被接受，见 4.3）；
增量更新（加/删一篇文档）未做；真实语料上的端到端未跑（语料未下载）。

## 8. 性能与复杂度

| 量 | 复杂度 / 实测 |
| --- | --- |
| 建索引 | 时间 O(总词次 + V log V)（V log V 来自 `index_save()` 的词条排序）；空间 O(V + 位置条目数) |
| 单词查询 | 时间 O(df)（要用到全部位置）；只查文档时是 O(df) 的顺序扫描 |
| 多词 AND | 时间 O(各词 df 之和)，升序数组的多路求交，无随机访问 |
| 短语查询 | 时间 O(min(df) × 平均单文档位置数 × 二分查找)；用 tf 最小词当锚点 |
| 落盘 / 加载 | `index.bin` 用 gap + LEB128，相邻增量通常 1 字节；加载是全量重建哈希表 |

合成语料实测：`index_gen` 0.79 s（两遍扫描 + 两次建索引 + 落盘 263.8 KB），
`query` 加载 + 一次查询 0.02 s；早期 88 万条三元组的样例上 `index_gen` 约 1.3 s、峰值内存约 39 MB。

## 9. Bonus：50 万文件、4 亿不同词

先把账算出来：词典（内存哈希表）4×10⁸ × ~20 B ≈ 8 GB（含装填因子 ~16 GB）；
postings 裸存 10¹⁰ × 4 B = 40 GB，delta + varint 后 ≈ 10 GB；文档表 500,000 × 64 B ≈ 32 MB。
⇒ 索引总量 10–20 GB，**必须落盘**，"全内存 + 定长数组"在这里不成立。

会当场死掉的写法（早期骨架里的）：把 `FileNode *file_list[5000000]`（38.1 MB）嵌在词表结构里
（每个词都要背 38 MB）、按"词数上限"开 `WordCount *words[400000000]`（2.98 GB）、
每个词条独立 `malloc` 一块 postings（4 亿次分配 + 指针追逐）。

能跑的做法（与第 4 节共用同一套接口，只换后端）：

1. **SPIMI / BSBI 外部归并建索引**：内存上限由缓冲决定，与 V、N 无关；两遍扫描的结构与本实现一致
   （只是 stoplist 必须先落盘，因为内存放不下整份索引）；
2. **磁盘倒排 + delta/varint + 每 128 条一个 skip 指针**（求交时跳跃前进）；
3. **排序块压缩词典 + 内存稀疏索引**（每块 64 个 term、块内前缀共享）：常驻内存从 ~10 GB 降到 ~100 MB；
4. **查询用 top-K（WAND / MaxScore）而不是全量返回** —— 这正是第 6 节"查询阈值"在 Bonus 规模下的必然形态；
5. `uint32_t` 装 docid / termid，文件偏移用 `uint64_t`，`mmap` + 顺序扫描；
6. 若 10 GB 仍放不下，把 postings 按 term 哈希分片成 P 个桶，各自独立建索引 / 查询。

分阶段路径：M1 单文件词计数（先对拍 `tr | sort | uniq -c`）→ M2 多文件 + docid 表 + df 统计 + θ 停用词
→ M3 查询（单词 / 交集 / 短语）+ τ 实验 → M4 SPIMI + varint/skip + 块压缩词典，
用脚本生成的 50 万小文件验证"峰值 RSS 与语料规模无关（曲线是平的）"。
**现状：M1–M3 已实现并测试通过（本文第 3–7 节），M4 未实现。**

## 10. 局限与后续工作

1. **真实语料未跑**：所有实测都在合成语料上。合成语料是"人造 Zipf 分布"，量级与形态接近真实文本
   （V=7,124、N=40、停用词占 V 的 5.9%），但不能替代真实语料上的结论；拿到
   `shakespeare.mit.edu` 的文本后 `./index_gen <每部剧一个文件>` 一步即可复用全部实验。
2. **文档粒度固定为"一个输入文件 = 一篇文档"**：想按幕 / 场切需要增加解析层（第 6.1 节的讨论）。
3. **停用词使短语查询在含功能词时不可用**（6.3 第 4 点），需要更深设计才能同时满足题面两条要求。
4. **无校验和**：想防住"结构自洽的篡改"需要 v2 格式。
5. **增量更新未实现**：当前是全量重建（题面未要求）。
6. **B+ 树 / 前缀压缩词典**只在 Bonus 讨论里，未落地。

## 11. 参考

- 题目原文：`image.png`（MIT 6.006 风格课后题，作者陈越，单位浙江大学）。
- Porter, M. F. *An algorithm for suffix stripping*, Program, 1980（`code/stem.c` 的实现依据；
  上游对照 `stmr.c` / `stmr.h`）。
- Manning, Raghavan, Schütze. *Introduction to Information Retrieval*（第 1–2 章：倒排索引、df 与停用词；
  第 4 章：gap 编码与变长编码；第 5 章：索引压缩；第 7 章：top-K 检索）。
- 题面允许 *"download the functions for handling stop words and stemming from the Internet"*，
  本实现的词干模块来自公开 Porter 实现，停用词识别是自己按 `df/N` 准则算出来的（语料相关，不能硬编码）。
