/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -m 10 ./iconv_open-aix.gperf  */
/* Computed positions: -k'4,$' */

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

#line 1 "./iconv_open-aix.gperf"
struct mapping { int standard_name; const char vendor_name[10 + 1]; };

#define TOTAL_KEYWORDS 32
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 44
/* maximum key range = 39, duplicates = 0 */

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
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45,  0,  4, 25,
       0, 11, 24,  9, 17,  3, 14, 21, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
       3, 45,  1, 45, 45, 45, 45,  0, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
      45, 45, 45, 45, 45, 45, 45, 45
    };
  return len + asso_values[(unsigned char)str[3]+2] + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str6[sizeof("EUC-TW")];
    char stringpool_str7[sizeof("EUC-KR")];
    char stringpool_str8[sizeof("CP852")];
    char stringpool_str9[sizeof("EUC-JP")];
    char stringpool_str10[sizeof("ISO-8859-2")];
    char stringpool_str11[sizeof("CP857")];
    char stringpool_str12[sizeof("CP850")];
    char stringpool_str13[sizeof("ISO-8859-7")];
    char stringpool_str14[sizeof("CP932")];
    char stringpool_str15[sizeof("GB2312")];
    char stringpool_str16[sizeof("BIG5")];
    char stringpool_str17[sizeof("CP437")];
    char stringpool_str19[sizeof("ISO-8859-5")];
    char stringpool_str20[sizeof("ISO-8859-15")];
    char stringpool_str21[sizeof("ISO-8859-3")];
    char stringpool_str22[sizeof("ISO-8859-13")];
    char stringpool_str23[sizeof("CP1046")];
    char stringpool_str24[sizeof("ISO-8859-8")];
    char stringpool_str25[sizeof("CP856")];
    char stringpool_str26[sizeof("CP1125")];
    char stringpool_str27[sizeof("ISO-8859-6")];
    char stringpool_str28[sizeof("CP865")];
    char stringpool_str29[sizeof("CP922")];
    char stringpool_str30[sizeof("CP1252")];
    char stringpool_str31[sizeof("ISO-8859-9")];
    char stringpool_str33[sizeof("CP943")];
    char stringpool_str34[sizeof("ISO-8859-4")];
    char stringpool_str35[sizeof("ISO-8859-1")];
    char stringpool_str38[sizeof("CP1129")];
    char stringpool_str40[sizeof("CP869")];
    char stringpool_str41[sizeof("CP1124")];
    char stringpool_str44[sizeof("CP861")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "EUC-TW",
    "EUC-KR",
    "CP852",
    "EUC-JP",
    "ISO-8859-2",
    "CP857",
    "CP850",
    "ISO-8859-7",
    "CP932",
    "GB2312",
    "BIG5",
    "CP437",
    "ISO-8859-5",
    "ISO-8859-15",
    "ISO-8859-3",
    "ISO-8859-13",
    "CP1046",
    "ISO-8859-8",
    "CP856",
    "CP1125",
    "ISO-8859-6",
    "CP865",
    "CP922",
    "CP1252",
    "ISO-8859-9",
    "CP943",
    "ISO-8859-4",
    "ISO-8859-1",
    "CP1129",
    "CP869",
    "CP1124",
    "CP861"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct mapping mappings[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 43 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str6, "IBM-eucTW"},
#line 42 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str7, "IBM-eucKR"},
#line 25 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str8, "IBM-852"},
#line 41 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str9, "IBM-eucJP"},
#line 14 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str10, "ISO8859-2"},
#line 27 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str11, "IBM-857"},
#line 24 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str12, "IBM-850"},
#line 19 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str13, "ISO8859-7"},
#line 33 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str14, "IBM-932"},
#line 40 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str15, "IBM-eucCN"},
#line 44 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str16, "big5"},
#line 23 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str17, "IBM-437"},
    {-1},
#line 17 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str19, "ISO8859-5"},
#line 22 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str20, "ISO8859-15"},
#line 15 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str21, "ISO8859-3"},
#line 31 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str22, "IBM-921"},
#line 35 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str23, "IBM-1046"},
#line 20 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str24, "ISO8859-8"},
#line 26 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str25, "IBM-856"},
#line 37 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str26, "IBM-1125"},
#line 18 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str27, "ISO8859-6"},
#line 29 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str28, "IBM-865"},
#line 32 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str29, "IBM-922"},
#line 39 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str30, "IBM-1252"},
#line 21 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str31, "ISO8859-9"},
    {-1},
#line 34 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str33, "IBM-943"},
#line 16 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str34, "ISO8859-4"},
#line 13 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str35, "ISO8859-1"},
    {-1}, {-1},
#line 38 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str38, "IBM-1129"},
    {-1},
#line 30 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str40, "IBM-869"},
#line 36 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str41, "IBM-1124"},
    {-1}, {-1},
#line 28 "./iconv_open-aix.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str44, "IBM-861"}
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
