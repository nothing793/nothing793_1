/* pr1（8-1 Roll Your Own Mini Search Engine）的词干提取模块。
 *
 * 对外提供两个接口：
 *     int  stem(char *p, int index, int position);   Porter 词干算法本体
 *     void stemword(char *word);                     转小写 + 词干提取的封装
 *
 * 建索引（词计数、倒排索引）与查询时对同一个词都要走这套处理，索引里存的是
 * 词干而不是原形，这样 "run" / "runs" / "running" 才会命中同一篇文档。
 *
 * 来源：https://github.com/wooorm/stmr.c
 *       Martin Porter 1980 年词干算法的 ANSI C 实现（MIT 许可，见仓库
 *       LICENSE-stmr.txt）。仓库根目录的 stmr.c / stmr.h 是上游原样副本，
 *       只作对照，编译本模块时既不需要也不要一起编译（符号会重复定义）。
 *
 * 调用约定：
 *   1. stem() 只处理小写字母序列，所以调用前要先用 tolower() 统一大小写；
 *   2. stem(p, 0, len - 1) 原地改写 p[0 .. len-1]，返回词干末字符的下标，
 *      不写结束符，调用方需要自己补 '\0'；
 *   3. 长度 ≤ 2 的字符串原样返回（此时返回值即 len - 1，补 '\0' 依然安全）；
 *   4. 内部使用文件级静态变量，不可重入、非线程安全（本项目单线程，无影响）。
 */

#include <ctype.h>
#include <string.h>

/* 上游 stmr.h 里的两个宏，改成独立模块后直接写在本文件里。 */
#define TRUE 1
#define FALSE 0

int stem(char *p, int index, int position);
void stemword(char *word);

/* 以下是 wooorm/stmr.c 的正文，逐行照搬（保留上游英文注释与算法出处），
 * 改动只有两处，都与算法结果无关：
 *   1. 文件级静态变量 b / k / k0 / j 改名为 stem_buf / stem_k / stem_k0 /
 *      stem_j，与 hw3.c 中的内联版本保持一致，方便两边对拍；
 *   2. isDoubleConsonant() 开头加了 `index <= stem_k0` 的边界守卫。
 *      上游在 index == k0 时会读 stem_buf[k0 - 1]，越出词首 1 字节
 *      （ASan：stmr.c:163 / hw3.c:208 / 本文件同处，均为 stack-buffer-overflow）。
 *      触发者是 step1ab 把词干削到一个字母的情形（"ied" -> i、"aing" -> a 等，
 *      即上游 Release 3 注释里提到的那些词）；那一步该函数必然返回假，
 *      所以守卫只是去掉越界读，不改变任何输出。
 * 除此之外算法逻辑、常量、--DEPARTURE-- 取舍全部保持原样。 */

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
  /* --DEPARTURE--（本文件新增的边界守卫）：下面那句要读 stem_buf[index - 1]，
   * 所以必须先保证 index >= k0 + 1。上游没有这个检查，当 index == k0 时会读到
   * 缓冲区前一个字节（越界读；stmr.c、hw3.c 同样存在）。
   * 该情况只出现在 step1ab 中 k = j 之后把词干削到一个字母时（"ied" -> i、
   * "aing" -> a 等），此时 index 与 k0 相等、函数必为假，故直接返回 FALSE：
   * 结果与上游一致，只不再越界。 */
  if (index <= stem_k0) {
    return FALSE;
  }

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
int
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

/* 转小写 + 词干提取，原地处理以 '\0' 结尾的字符串（逻辑与 hw3.c 的
 * stemword() 相同）。空串直接返回，不调用 stem()。 */
void stemword(char *word)
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

    /* stem() 返回词干末字符的下标，后面那个字节要自己补成结束符。 */
    word[stem(word, 0, len - 1) + 1] = '\0';
}
