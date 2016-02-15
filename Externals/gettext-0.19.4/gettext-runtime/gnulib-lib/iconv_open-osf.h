/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -m 10 ./iconv_open-osf.gperf  */
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

#line 1 "./iconv_open-osf.gperf"
struct mapping { int standard_name; const char vendor_name[10 + 1]; };

#define TOTAL_KEYWORDS 38
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 47
/* maximum key range = 42, duplicates = 0 */

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
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48,  2, 29,
      24, 34, 31,  0, 15, 14, 10, 13,  2, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48,  7, 48, 48, 48, 48, 48, 48,
      11, 48,  2,  7, 48, 48, 48,  1, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
      48, 48, 48, 48, 48, 48, 48, 48, 48
    };
  return len + asso_values[(unsigned char)str[3]+3] + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str6[sizeof("CP1255")];
    char stringpool_str7[sizeof("CP775")];
    char stringpool_str8[sizeof("CP1250")];
    char stringpool_str9[sizeof("EUC-TW")];
    char stringpool_str10[sizeof("EUC-KR")];
    char stringpool_str11[sizeof("TIS-620")];
    char stringpool_str12[sizeof("ISO-8859-5")];
    char stringpool_str13[sizeof("ISO-8859-15")];
    char stringpool_str14[sizeof("BIG5")];
    char stringpool_str15[sizeof("CP855")];
    char stringpool_str16[sizeof("CP1258")];
    char stringpool_str17[sizeof("CP850")];
    char stringpool_str18[sizeof("CP865")];
    char stringpool_str19[sizeof("EUC-JP")];
    char stringpool_str20[sizeof("CP1257")];
    char stringpool_str21[sizeof("CP1256")];
    char stringpool_str22[sizeof("ISO-8859-8")];
    char stringpool_str23[sizeof("SHIFT_JIS")];
    char stringpool_str25[sizeof("ISO-8859-9")];
    char stringpool_str26[sizeof("ISO-8859-7")];
    char stringpool_str27[sizeof("ISO-8859-6")];
    char stringpool_str29[sizeof("CP857")];
    char stringpool_str30[sizeof("CP1252")];
    char stringpool_str31[sizeof("CP869")];
    char stringpool_str32[sizeof("CP949")];
    char stringpool_str33[sizeof("CP866")];
    char stringpool_str34[sizeof("CP437")];
    char stringpool_str35[sizeof("CP1251")];
    char stringpool_str36[sizeof("ISO-8859-2")];
    char stringpool_str37[sizeof("CP1254")];
    char stringpool_str38[sizeof("CP874")];
    char stringpool_str39[sizeof("CP852")];
    char stringpool_str40[sizeof("CP1253")];
    char stringpool_str41[sizeof("ISO-8859-1")];
    char stringpool_str42[sizeof("CP862")];
    char stringpool_str43[sizeof("ISO-8859-4")];
    char stringpool_str46[sizeof("ISO-8859-3")];
    char stringpool_str47[sizeof("CP861")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "CP1255",
    "CP775",
    "CP1250",
    "EUC-TW",
    "EUC-KR",
    "TIS-620",
    "ISO-8859-5",
    "ISO-8859-15",
    "BIG5",
    "CP855",
    "CP1258",
    "CP850",
    "CP865",
    "EUC-JP",
    "CP1257",
    "CP1256",
    "ISO-8859-8",
    "SHIFT_JIS",
    "ISO-8859-9",
    "ISO-8859-7",
    "ISO-8859-6",
    "CP857",
    "CP1252",
    "CP869",
    "CP949",
    "CP866",
    "CP437",
    "CP1251",
    "ISO-8859-2",
    "CP1254",
    "CP874",
    "CP852",
    "CP1253",
    "ISO-8859-1",
    "CP862",
    "ISO-8859-4",
    "ISO-8859-3",
    "CP861"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct mapping mappings[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 41 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str6, "cp1255"},
#line 24 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str7, "cp775"},
#line 36 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str8, "cp1250"},
#line 47 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str9, "eucTW"},
#line 46 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str10, "eucKR"},
#line 50 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str11, "TACTIS"},
#line 17 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str12, "ISO8859-5"},
#line 22 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str13, "ISO8859-15"},
#line 48 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str14, "big5"},
#line 27 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str15, "cp855"},
#line 44 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str16, "cp1258"},
#line 25 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str17, "cp850"},
#line 31 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str18, "cp865"},
#line 45 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str19, "eucJP"},
#line 43 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str20, "cp1257"},
#line 42 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str21, "cp1256"},
#line 20 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str22, "ISO8859-8"},
#line 49 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str23, "SJIS"},
    {-1},
#line 21 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str25, "ISO8859-9"},
#line 19 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str26, "ISO8859-7"},
#line 18 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str27, "ISO8859-6"},
    {-1},
#line 28 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str29, "cp857"},
#line 38 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str30, "cp1252"},
#line 33 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str31, "cp869"},
#line 35 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str32, "KSC5601"},
#line 32 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str33, "cp866"},
#line 23 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str34, "cp437"},
#line 37 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str35, "cp1251"},
#line 14 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str36, "ISO8859-2"},
#line 40 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str37, "cp1254"},
#line 34 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str38, "cp874"},
#line 26 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str39, "cp852"},
#line 39 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str40, "cp1253"},
#line 13 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str41, "ISO8859-1"},
#line 30 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str42, "cp862"},
#line 16 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str43, "ISO8859-4"},
    {-1}, {-1},
#line 15 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str46, "ISO8859-3"},
#line 29 "./iconv_open-osf.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str47, "cp861"}
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
