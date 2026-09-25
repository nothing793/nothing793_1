/* 7-1 Document Distance
 *
 * 文档距离用词频向量之间的夹角度量：
 *     (F(D1),F(D2)) = Σ F1(w)·F2(w)
 *     |F(D)|        = sqrt((F(D),F(D)))
 *     θ(D1,D2)      = arccos( (F(D1),F(D2)) / (|F(D1)|·|F(D2)|) ) ∈ [0,π/2]
 *
 * 实现要点：
 *   1. 词的定义是「连续的数字/字母序列」，所以按字符扫描文本把它切成 alnum 段，
 *      标点自然成为分隔符（"don't" → don/t，"well-known" → well/known）。
 *   2. 大小写不敏感：先统一转小写，再交给内联的 Porter 词干算法（见下方
 *      stmr 段）处理 es/ed/ing/ies 等后缀。停用词照常统计（题面要求）。
 *   3. 每个文档用「哈希表 + 单链表」存 (词, 频次)，插入与查找都是 O(1) 均摊。
 *   4. N ≤ 100，文件对最多 C(100,2) = 4950 个，先把所有点积和 2-范数算好；
 *      查询 M ≤ 100000，因此每次询问必须 O(1)（逐个询问现算会超时）。
 */

#define _POSIX_C_SOURCE 200809L /* 为了 getline() */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TITLE 6                     /* 题名最长 6 个字符 */
#define TITLE_BUF (MAX_TITLE + 2)
#define MAX_WORD 20                     /* 题面保证单词不超过 20 个字符 */
#define WORD_BUF (MAX_WORD + 1)
#define BUCKETS 2048                    /* 每个文档的哈希桶数 */
#define TITLE_BUCKETS 256               /* 题名 -> 文档下标 的哈希桶数 */
#define PI_2 1.57079632679489661923     /* π/2，空文档与非空文档的夹角 */

/* 以下至 stem() 结束的代码内联自 https://github.com/wooorm/stmr.c
 * （Martin Porter 1980 词干算法的 ANSI C 实现，MIT 许可，见仓库 LICENSE-stmr.txt）。
 * 内联时只做了三处与算法无关的调整：
 *   1. 去掉 stmr.h 的 include，TRUE / FALSE 宏与 stem() 原型在下面给出；
 *   2. stem() 改为 static（单文件不需要导出）；
 *   3. 文件级静态变量 b / k / k0 / j 改名为 stem_buf / stem_k / stem_k0 / stem_j，
 *      以免与 hw3.c 里的同名局部变量遮蔽（下面注释中的旧名与之对应）。
 * 算法逻辑、常量、--DEPARTURE-- 取舍全部保持原样。 */

#define TRUE 1
#define FALSE 0

static int stem(char *p, int index, int position);

/* This is the Porter stemming algorithm, coded up in ANSI C by the
 * author. It may be be regarded as canonical, in that it follows the
 * algorithm presented in
 *
 * Porter, 1980, An algorithm for suffix stripping, Program, Vol. 14,
 * no. 3, pp 130-137,
 *
 * only differing from it at the points marked --DEPARTURE-- below.
 *
 * See also http://www.tartarus.org/~martin/PorterStemmer
 *
 * The algorithm as described in the paper could be exactly replicated
 * by adjusting the points of DEPARTURE, but this is barely necessary,
 * because (a) the points of DEPARTURE are definitely improvements, and
 * (b) no encoding of the Porter stemmer I have seen is anything like
 * as exact as this version, even with the points of DEPARTURE!
 *
 * You can compile it on Unix with 'gcc -O3 -o stem stem.c' after which
 * 'stem' takes a list of inputs and sends the stemmed equivalent to
 * stdout.
 *
 * The algorithm as encoded here is particularly fast.
 *
 * Release 1: was many years ago
 * Release 2: 11 Apr 2013
 *     fixes a bug noted by Matt Patenaude <matt@mattpatenaude.com>,
 *
 *     case 'o': if (ends("\03" "ion") && (b[j] == 's' || b[j] == 't')) break;
 *         ==>
 *     case 'o': if (ends("\03" "ion") && j >= k0 && (b[j] == 's' || b[j] == 't')) break;
 *
 *     to avoid accessing b[k0-1] when the word in b is "ion".
 * Release 3: 25 Mar 2014
 *     fixes a similar bug noted by Klemens Baum <klemensbaum@gmail.com>,
 *     that if step1ab leaves a one letter result (ied -> i, aing -> a etc),
 *     step2 and step4 access the byte before the first letter. So we skip
 *     steps after step1ab unless k > k0. */

/* The main part of the stemming algorithm starts here. b is a buffer
 * holding a word to be stemmed. The letters are in b[k0], b[k0+1] ...
 * ending at b[k]. In fact k0 = 0 in this demo program. k is readjusted
 * downwards as the stemming progresses. Zero termination is not in fact
 * used in the algorithm.
 *
 * Note that only lower case sequences are stemmed. Forcing to lower case
 * should be done before stem(...) is called. */

/* buffer for word to be stemmed */
static char *stem_buf;

static int stem_k;
static int stem_k0;

/* j is a general offset into the string */
static int stem_j;

/**
 * TRUE when `b[i]` is a consonant.
 */

static int
isConsonant(int index) {
  switch (stem_buf[index]) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
      return FALSE;
    case 'y':
      return (index == stem_k0) ? TRUE : !isConsonant(index - 1);
    default:
      return TRUE;
  }
}

/* Measure the number of consonant sequences between
 * `k0` and `j`.  If C is a consonant sequence and V
 * a vowel sequence, and <..> indicates arbitrary
 * presence:
 *
 *   <C><V>       gives 0
 *   <C>VC<V>     gives 1
 *   <C>VCVC<V>   gives 2
 *   <C>VCVCVC<V> gives 3
 *   ....
 */
static int
getMeasure() {
  int position;
  int index;

  position = 0;
  index = stem_k0;

  while (TRUE) {
    if (index > stem_j) {
      return position;
    }

    if (!isConsonant(index)) {
      break;
    }

    index++;
  }

  index++;

  while (TRUE) {
    while (TRUE) {
      if (index > stem_j) {
        return position;
      }

      if (isConsonant(index)) {
        break;
      }

      index++;
    }

    index++;
    position++;

    while (TRUE) {
      if (index > stem_j) {
        return position;
      }

      if (!isConsonant(index)) {
        break;
      }

      index++;
    }

    index++;
  }
}

/* `TRUE` when `k0, ... j` contains a vowel. */
static int
vowelInStem() {
  int index;

  index = stem_k0 - 1;

  while (++index <= stem_j) {
    if (!isConsonant(index)) {
      return TRUE;
    }
  }

  return FALSE;
}

/* `TRUE` when `j` and `(j-1)` are the same consonant. */
static int
isDoubleConsonant(int index) {
  if (stem_buf[index] != stem_buf[index - 1]) {
    return FALSE;
  }

  return isConsonant(index);
}

/* `TRUE` when `i - 2, i - 1, i` has the form
 * `consonant - vowel - consonant` and also if the second
 * C is not `"w"`, `"x"`, or `"y"`. this is used when
 * trying to restore an `e` at the end of a short word.
 *
 * Such as:
 *
 * `cav(e)`, `lov(e)`, `hop(e)`, `crim(e)`, but `snow`,
 * `box`, `tray`.
 */
static int
cvc(int index) {
  int character;

  if (index < stem_k0 + 2 || !isConsonant(index) || isConsonant(index - 1) || !isConsonant(index - 2)) {
    return FALSE;
  }

  character = stem_buf[index];

  if (character == 'w' || character == 'x' || character == 'y') {
    return FALSE;
  }

  return TRUE;
}

/* `ends(s)` is `TRUE` when `k0, ...k` ends with `value`. */
static int
ends(const char *value) {
  int length = value[0];

  /* Tiny speed-up. */
  if (value[length] != stem_buf[stem_k]) {
    return FALSE;
  }

  if (length > stem_k - stem_k0 + 1) {
    return FALSE;
  }

  if (memcmp(stem_buf + stem_k - length + 1, value + 1, length) != 0) {
    return FALSE;
  }

  stem_j = stem_k - length;

  return TRUE;
}

/* `setTo(value)` sets `(j + 1), ...k` to the characters in
 * `value`, readjusting `k`. */
static void
setTo(const char *value) {
  int length = value[0];

  memmove(stem_buf + stem_j + 1, value + 1, length);

  stem_k = stem_j + length;
}

/* Set string. */
static void
replace(const char *value) {
  if (getMeasure() > 0) {
    setTo(value);
  }
}

/* `step1ab()` gets rid of plurals, `-ed`, `-ing`.
 *
 * Such as:
 *
 *   caresses  ->  caress
 *   ponies    ->  poni
 *   ties      ->  ti
 *   caress    ->  caress
 *   cats      ->  cat
 *
 *   feed      ->  feed
 *   agreed    ->  agree
 *   disabled  ->  disable
 *
 *   matting   ->  mat
 *   mating    ->  mate
 *   meeting   ->  meet
 *   milling   ->  mill
 *   messing   ->  mess
 *
 *   meetings  ->  meet
 */
static void
step1ab() {
  int character;

  if (stem_buf[stem_k] == 's') {
    if (ends("\04" "sses")) {
      stem_k -= 2;
    } else if (ends("\03" "ies")) {
      setTo("\01" "i");
    } else if (stem_buf[stem_k - 1] != 's') {
      stem_k--;
    }
  }

  if (ends("\03" "eed")) {
    if (getMeasure() > 0) {
      stem_k--;
    }
  } else if ((ends("\02" "ed") || ends("\03" "ing")) && vowelInStem()) {
    stem_k = stem_j;

    if (ends("\02" "at")) {
      setTo("\03" "ate");
    } else if (ends("\02" "bl")) {
      setTo("\03" "ble");
    } else if (ends("\02" "iz")) {
      setTo("\03" "ize");
    } else if (isDoubleConsonant(stem_k)) {
      stem_k--;

      character = stem_buf[stem_k];

      if (character == 'l' || character == 's' || character == 'z') {
        stem_k++;
      }
    } else if (getMeasure() == 1 && cvc(stem_k)) {
      setTo("\01" "e");
    }
  }
}

/* `step1c()` turns terminal `"y"` to `"i"` when there
 * is another vowel in the stem. */
static void
step1c() {
  if (ends("\01" "y") && vowelInStem()) {
    stem_buf[stem_k] = 'i';
  }
}

/* `step2()` maps double suffices to single ones.
 * so -ization ( = -ize plus -ation) maps to -ize etc.
 * note that the string before the suffix must give
 * getMeasure() > 0. */
static void
step2() {
  switch (stem_buf[stem_k - 1]) {
    case 'a':
      if (ends("\07" "ational")) {
        replace("\03" "ate");
        break;
      }

      if (ends("\06" "tional")) {
        replace("\04" "tion");
        break;
      }

      break;
    case 'c':
      if (ends("\04" "enci")) {
        replace("\04" "ence");
        break;
      }

      if (ends("\04" "anci")) {
        replace("\04" "ance");
        break;
      }

      break;
    case 'e':
      if (ends("\04" "izer")) {
        replace("\03" "ize");
        break;
      }

      break;
    case 'l':
      /* --DEPARTURE--: To match the published algorithm,
       * replace this line with:
       *
       * ```
       * if (ends("\04" "abli")) {
       *     replace("\04" "able");
       *
       *     break;
       * }
       * ```
       */
      if (ends("\03" "bli")) {
        replace("\03" "ble");
        break;
      }

      if (ends("\04" "alli")) {
        replace("\02" "al");
        break;
      }

      if (ends("\05" "entli")) {
        replace("\03" "ent");
        break;
      }

      if (ends("\03" "eli")) {
        replace("\01" "e");
        break;
      }

      if (ends("\05" "ousli")) {
        replace("\03" "ous");
        break;
      }

      break;
    case 'o':
      if (ends("\07" "ization")) {
        replace("\03" "ize");
        break;
      }

      if (ends("\05" "ation")) {
        replace("\03" "ate");
        break;
      }

      if (ends("\04" "ator")) {
        replace("\03" "ate");
        break;
      }

      break;
    case 's':
      if (ends("\05" "alism")) {
        replace("\02" "al");
        break;
      }

      if (ends("\07" "iveness")) {
        replace("\03" "ive");
        break;
      }

      if (ends("\07" "fulness")) {
        replace("\03" "ful");
        break;
      }

      if (ends("\07" "ousness")) {
        replace("\03" "ous");
        break;
      }

      break;
    case 't':
      if (ends("\05" "aliti")) {
        replace("\02" "al");
        break;
      }

      if (ends("\05" "iviti")) {
        replace("\03" "ive");
        break;
      }

      if (ends("\06" "biliti")) {
        replace("\03" "ble");
        break;
      }

      break;
    /* --DEPARTURE--: To match the published algorithm, delete this line. */
    case 'g':
      if (ends("\04" "logi")) {
        replace("\03" "log");
        break;
      }
  }
}

/* `step3()` deals with -ic-, -full, -ness etc.
 * similar strategy to step2. */
static void
step3() {
  switch (stem_buf[stem_k]) {
    case 'e':
      if (ends("\05" "icate")) {
        replace("\02" "ic");
        break;
      }

      if (ends("\05" "ative")) {
        replace("\00" "");
        break;
      }

      if (ends("\05" "alize")) {
        replace("\02" "al");
        break;
      }

      break;
    case 'i':
      if (ends("\05" "iciti")) {
        replace("\02" "ic");
        break;
      }

      break;
    case 'l':
      if (ends("\04" "ical")) {
        replace("\02" "ic");
        break;
      }

      if (ends("\03" "ful")) {
        replace("\00" "");
        break;
      }

      break;
    case 's':
      if (ends("\04" "ness")) {
        replace("\00" "");
        break;
      }

      break;
  }
}

/* `step4()` takes off -ant, -ence etc., in
 * context <c>vcvc<v>. */
static void
step4() {
  switch (stem_buf[stem_k - 1]) {
    case 'a':
      if (ends("\02" "al")) {
        break;
      }

      return;
    case 'c':
      if (ends("\04" "ance")) {
        break;
      }

      if (ends("\04" "ence")) {
        break;
      }

      return;
    case 'e':
      if (ends("\02" "er")) {
        break;
      }

      return;
    case 'i':
      if (ends("\02" "ic")) {
        break;
      }

      return;
    case 'l':
      if (ends("\04" "able")) {
        break;
      }

      if (ends("\04" "ible")) {
        break;
      }

      return;
    case 'n':
      if (ends("\03" "ant")) {
        break;
      }

      if (ends("\05" "ement")) {
        break;
      }

      if (ends("\04" "ment")) {
        break;
      }

      if (ends("\03" "ent")) {
        break;
      }

      return;
    case 'o':
      if (ends("\03" "ion") && stem_j >= stem_k0 && (stem_buf[stem_j] == 's' || stem_buf[stem_j] == 't')) {
        break;
      }

      /* takes care of -ous */
      if (ends("\02" "ou")) {
        break;
      }

      return;
    case 's':
      if (ends("\03" "ism")) {
        break;
      }

      return;
    case 't':
      if (ends("\03" "ate")) {
        break;
      }

      if (ends("\03" "iti")) {
        break;
      }

      return;
    case 'u':
      if (ends("\03" "ous")) {
        break;
      }

      return;
    case 'v':
      if (ends("\03" "ive")) {
        break;
      }

      return;
    case 'z':
      if (ends("\03" "ize")) {
        break;
      }

      return;
    default:
      return;
  }

  if (getMeasure() > 1) {
    stem_k = stem_j;
  }
}

/* `step5()` removes a final `-e` if `getMeasure()` is
 * greater than `1`, and changes `-ll` to `-l` if
 * `getMeasure()` is greater than `1`. */
static void
step5() {
  int a;

  stem_j = stem_k;

  if (stem_buf[stem_k] == 'e') {
    a = getMeasure();

    if (a > 1 || (a == 1 && !cvc(stem_k - 1))) {
      stem_k--;
    }
  }

  if (stem_buf[stem_k] == 'l' && isDoubleConsonant(stem_k) && getMeasure() > 1) {
    stem_k--;
  }
}

/* In `stem(p, i, j)`, `p` is a `char` pointer, and the
 * string to be stemmed is from `p[i]` to
 * `p[j]` (inclusive).
 *
 * Typically, `i` is zero and `j` is the offset to the
 * last character of a string, `(p[j + 1] == '\0')`.
 * The stemmer adjusts the characters `p[i]` ... `p[j]`
 * and returns the new end-point of the string, `k`.
 *
 * Stemming never increases word length, so `i <= k <= j`.
 *
 * To turn the stemmer into a module, declare 'stem' as
 * extern, and delete the remainder of this file. */
static int
stem(char *p, int index, int position) {
  /* Copy the parameters into statics. */
  stem_buf = p;
  stem_k = position;
  stem_k0 = index;

  if (stem_k <= stem_k0 + 1) {
    return stem_k; /* --DEPARTURE-- */
  }

  /* With this line, strings of length 1 or 2 don't
   * go through the stemming process, although no
   * mention is made of this in the published
   * algorithm. Remove the line to match the published
   * algorithm. */
  step1ab();

  if (stem_k > stem_k0) {
    step1c();
    step2();
    step3();
    step4();
    step5();
  }

  return stem_k;
}

typedef struct WordEntry
{
    char word[WORD_BUF];
    int freq;
    struct WordEntry *bucketnext;       /* 同一个哈希桶里的下一个词 */
    struct WordEntry *listnext;         /* 本文档所有词（遍历用） */
} WordEntry;

typedef struct Document
{
    char title[TITLE_BUF];
    int size;                           /* 不同单词数 */
    WordEntry **bucket;                 /* BUCKETS 个桶头 */
    WordEntry *list;                    /* 所有单词的链表头 */
} Document;

/* djb2 字符串哈希 */
static unsigned int hashstring(const char *s)
{
    unsigned int h = 5381u;

    while(*s != '\0')
    {
        h = h * 33u + (unsigned char)*s;
        s++;
    }
    return h;
}


/* 转小写 + 词干提取。
 * stmr.c 的 stem() 只处理小写字母序列、原地修改缓冲区、返回词干末字符下标，
 * 且长度 ≤ 2 的字符串直接原样返回，所以补 '\0' 是安全的。 */
static void stemword(char *word)
{
    int len = 0;

    while(word[len] != '\0')
    {
        word[len] = (char)tolower((unsigned char)word[len]);
        len++;
    }

    if(len == 0)
    {
        return;
    }

    word[stem(word, 0, len - 1) + 1] = '\0';
}

static WordEntry *lookup(const Document *d, const char *word)
{
    unsigned int h = hashstring(word) % BUCKETS;
    WordEntry *p;

    for(p = d->bucket[h]; p != NULL; p = p->bucketnext)
    {
        if(strcmp(p->word, word) == 0)
        {
            return p;
        }
    }
    return NULL;
}

/* 词频 +1，没有就插入（word 已经过 stemword 处理） */
static void addword(Document *d, const char *word)
{
    unsigned int h = hashstring(word) % BUCKETS;
    WordEntry *p = lookup(d, word);

    if(p != NULL)
    {
        p->freq++;
        return;
    }

    p = (WordEntry *)malloc(sizeof(WordEntry));
    if(p == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }

    strcpy(p->word, word);
    p->freq = 1;
    p->bucketnext = d->bucket[h];
    d->bucket[h] = p;
    p->listnext = d->list;
    d->list = p;
    d->size++;
}

/* 把一行文本切成 alnum 段，逐段加入文档 */
static void scanline(Document *d, const char *line)
{
    char word[WORD_BUF];
    int len = 0;

    for(; *line != '\0'; line++)
    {
        unsigned char c = (unsigned char)*line;

        if(isalnum(c))
        {
            if(len < MAX_WORD) /* 超过 20 个字符的单词按题面不会出现，截断防御 */
            {
                word[len++] = (char)c;
            }
        }
        else if(len > 0)
        {
            word[len] = '\0';
            stemword(word);
            addword(d, word);
            len = 0;
        }
    }

    if(len > 0)
    {
        word[len] = '\0';
        stemword(word);
        addword(d, word);
    }
}

/* 文件块以「单独一行 #」结束 */
static int isterminator(const char *line)
{
    while(*line == ' ' || *line == '\t')
    {
        line++;
    }

    if(*line != '#')
    {
        return 0;
    }

    line++;
    while(*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
    {
        line++;
    }
    return *line == '\0';
}

static void initdocument(Document *d)
{
    d->title[0] = '\0';
    d->size = 0;
    d->list = NULL;
    d->bucket = (WordEntry **)calloc(BUCKETS, sizeof(WordEntry *));
    if(d->bucket == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
}

static void freedocument(Document *d)
{
    WordEntry *p = d->list;

    while(p != NULL)
    {
        WordEntry *next = p->listnext;
        free(p);
        p = next;
    }

    free(d->bucket);
    d->bucket = NULL;
    d->list = NULL;
    d->size = 0;
}

/* 点积 (F(a),F(b))：遍历词较少的文档，在另一篇里查表 */
static double dotproduct(const Document *a, const Document *b)
{
    const Document *outer = (a->size <= b->size) ? a : b;
    const Document *inner = (outer == a) ? b : a;
    const WordEntry *p;
    double sum = 0.0;

    for(p = outer->list; p != NULL; p = p->listnext)
    {
        const WordEntry *q = lookup(inner, p->word);

        if(q != NULL)
        {
            sum += (double)p->freq * (double)q->freq;
        }
    }
    return sum;
}

static int finddocument(const Document *docs, const int *titlehead,
                        const int *titlenext, const char *title)
{
    /* titlehead 里存的是「下标 + 1」，0 表示空桶 */
    int t;

    for(t = titlehead[hashstring(title) % TITLE_BUCKETS]; t != 0; t = titlenext[t - 1])
    {
        if(strcmp(docs[t - 1].title, title) == 0)
        {
            return t - 1;
        }
    }
    return -1;
}

int main(void)
{
    int n, m, i, j, q;
    Document *docs;
    double *dot;
    double *norm;
    int *titlehead;
    int *titlenext;
    char *line = NULL;
    size_t linecap = 0;
    char title1[TITLE_BUF];
    char title2[TITLE_BUF];

    if(scanf("%d", &n) != 1 || n <= 0)
    {
        return 0;
    }

    docs = (Document *)malloc((size_t)n * sizeof(Document));
    if(docs == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    /* 读入 N 个文件块：第一行题名，随后各行是正文，直到单独一行的 # */
    for(i = 0; i < n; i++)
    {
        initdocument(&docs[i]);

        if(scanf("%7s", docs[i].title) != 1)
        {
            fprintf(stderr, "bad input: missing title\n");
            return EXIT_FAILURE;
        }

        while(getline(&line, &linecap, stdin) != -1)
        {
            if(isterminator(line))
            {
                break;
            }
            scanline(&docs[i], line);
        }
    }
    free(line);
    line = NULL;

    /* 点积矩阵与 2-范数：最多 4950 个文件对，先全部算好 */
    dot = (double *)malloc((size_t)n * n * sizeof(double));
    norm = (double *)malloc((size_t)n * sizeof(double));
    if(dot == NULL || norm == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    for(i = 0; i < n; i++)
    {
        for(j = 0; j <= i; j++)
        {
            double v = dotproduct(&docs[i], &docs[j]);

            dot[(size_t)i * n + j] = v;
            dot[(size_t)j * n + i] = v;
        }
        norm[i] = sqrt(dot[(size_t)i * n + i]);
    }

    /* 题名 -> 文档下标 的哈希表 */
    titlehead = (int *)calloc(TITLE_BUCKETS, sizeof(int));
    titlenext = (int *)malloc((size_t)n * sizeof(int));
    if(titlehead == NULL || titlenext == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    for(i = 0; i < n; i++)
    {
        unsigned int h = hashstring(docs[i].title) % TITLE_BUCKETS;

        titlenext[i] = titlehead[h];
        titlehead[h] = i + 1;
    }

    if(scanf("%d", &m) != 1)
    {
        m = 0;
    }

    for(q = 0; q < m; q++)
    {
        int a, b;
        double cosine, theta;

        if(scanf("%7s %7s", title1, title2) != 2)
        {
            break;
        }

        a = finddocument(docs, titlehead, titlenext, title1);
        b = finddocument(docs, titlehead, titlenext, title2);

        if(a < 0 || b < 0) /* 题面保证询问的文件都存在，这里仅作防御 */
        {
            printf("Case %d: 0.000\n", q + 1);
            continue;
        }

        if(norm[a] > 0.0 && norm[b] > 0.0)
        {
            cosine = dot[(size_t)a * n + b] / (norm[a] * norm[b]);
        }
        else if(norm[a] > 0.0 || norm[b] > 0.0)
        {
            cosine = 0.0; /* 一篇为空：向量夹角为 π/2 */
        }
        else
        {
            cosine = 1.0; /* 两篇都为空：约定夹角为 0 */
        }

        /* 浮点误差可能让余弦值稍微超出 [-1,1]，夹一下避免 acos 得到 nan */
        if(cosine > 1.0)
        {
            cosine = 1.0;
        }
        else if(cosine < -1.0)
        {
            cosine = -1.0;
        }

        theta = acos(cosine);
        printf("Case %d: %.3f\n", q + 1, theta);
    }

    for(i = 0; i < n; i++)
    {
        freedocument(&docs[i]);
    }
    free(docs);
    free(dot);
    free(norm);
    free(titlehead);
    free(titlenext);

    return 0;
}
