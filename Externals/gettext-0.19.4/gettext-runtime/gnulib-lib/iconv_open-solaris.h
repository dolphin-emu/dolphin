/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -m 10 ./iconv_open-solaris.gperf  */
/* Computed positions: -k'10' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "./iconv_open-solaris.gperf"
struct mapping { int standard_name; const char vendor_name[10 + 1]; };

#define TOTAL_KEYWORDS 13
#define MIN_WORD_LENGTH 5
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 5
#define MAX_HASH_VALUE 19
/* maximum key range = 15, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
mapping_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20,  0,
       9,  8,  7,  6,  5,  4,  3,  2, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
        break;
    }
  return hval;
}

struct stringpool_t
  {
    char stringpool_str5[sizeof("ASCII")];
    char stringpool_str6[sizeof("CP1251")];
    char stringpool_str7[sizeof("$   abc")];
    char stringpool_str10[sizeof("ISO-8859-1")];
    char stringpool_str11[sizeof("ISO-8859-15")];
    char stringpool_str12[sizeof("ISO-8859-9")];
    char stringpool_str13[sizeof("ISO-8859-8")];
    char stringpool_str14[sizeof("ISO-8859-7")];
    char stringpool_str15[sizeof("ISO-8859-6")];
    char stringpool_str16[sizeof("ISO-8859-5")];
    char stringpool_str17[sizeof("ISO-8859-4")];
    char stringpool_str18[sizeof("ISO-8859-3")];
    char stringpool_str19[sizeof("ISO-8859-2")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "ASCII",
    "CP1251",
    "$   abc",
    "ISO-8859-1",
    "ISO-8859-15",
    "ISO-8859-9",
    "ISO-8859-8",
    "ISO-8859-7",
    "ISO-8859-6",
    "ISO-8859-5",
    "ISO-8859-4",
    "ISO-8859-3",
    "ISO-8859-2"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct mapping mappings[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 19 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str5, "646"},
#line 30 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str6, "ansi-1251"},
#line 18 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str7},
    {-1}, {-1},
#line 20 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str10, "ISO8859-1"},
#line 29 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str11, "ISO8859-15"},
#line 28 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str12, "ISO8859-9"},
#line 27 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str13, "ISO8859-8"},
#line 26 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str14, "ISO8859-7"},
#line 25 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str15, "ISO8859-6"},
#line 24 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str16, "ISO8859-5"},
#line 23 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str17, "ISO8859-4"},
#line 22 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str18, "ISO8859-3"},
#line 21 "./iconv_open-solaris.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str19, "ISO8859-2"}
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct mapping *
mapping_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = mapping_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int o = mappings[key].standard_name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &mappings[key];
            }
        }
    }
  return 0;
}
