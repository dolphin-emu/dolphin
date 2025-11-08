/* ANSI-C code produced by gperf version 3.2 */
/* Command-line: gperf -m 10 lib/aliases_syshpux.gperf  */
/* Computed positions: -k'1,3-11,$' */

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
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "lib/aliases_syshpux.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 361
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 19
#define MAX_HASH_VALUE 1033
/* maximum key range = 1015, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
aliases_hash (register const char *str, register size_t len)
{
  static const unsigned short asso_values[] =
    {
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034,    2,   22, 1034,   56,    4,
        36,   62,   23,   18,   11,  100,   30,    3,  115, 1034,
      1034, 1034, 1034, 1034, 1034,   62,  170,  101,    3,   35,
        78,   78,  169,    3,  375,   55,   16,  149,   18,    8,
         3, 1034,    2,    4,  161,  210,  136,  158,  191,    6,
         5, 1034, 1034, 1034, 1034,    3, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034,
      1034, 1034, 1034, 1034, 1034, 1034, 1034, 1034
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 9:
        hval += asso_values[(unsigned char)str[8]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
#if defined __cplusplus && (__cplusplus >= 201703L || (__cplusplus >= 201103L && defined __clang_major__ && defined __clang_minor__ && __clang_major__ + (__clang_minor__ >= 9) > 3))
      [[fallthrough]];
#elif defined __GNUC__ && __GNUC__ >= 7
      __attribute__ ((__fallthrough__));
#endif
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str19[sizeof("SJIS")];
    char stringpool_str22[sizeof("L1")];
    char stringpool_str29[sizeof("L6")];
    char stringpool_str34[sizeof("R8")];
    char stringpool_str36[sizeof("L5")];
    char stringpool_str41[sizeof("L4")];
    char stringpool_str43[sizeof("ISO-IR-199")];
    char stringpool_str48[sizeof("L8")];
    char stringpool_str50[sizeof("ISO-IR-6")];
    char stringpool_str54[sizeof("L2")];
    char stringpool_str55[sizeof("866")];
    char stringpool_str58[sizeof("ISO-IR-159")];
    char stringpool_str63[sizeof("ISO-IR-149")];
    char stringpool_str67[sizeof("ISO-IR-166")];
    char stringpool_str76[sizeof("PT154")];
    char stringpool_str79[sizeof("ISO-IR-14")];
    char stringpool_str80[sizeof("L3")];
    char stringpool_str81[sizeof("ISO-IR-165")];
    char stringpool_str92[sizeof("ISO-IR-126")];
    char stringpool_str96[sizeof("ISO-IR-109")];
    char stringpool_str98[sizeof("ISO-IR-101")];
    char stringpool_str100[sizeof("KOI8-R")];
    char stringpool_str103[sizeof("ISO-IR-144")];
    char stringpool_str105[sizeof("862")];
    char stringpool_str106[sizeof("ISO88599")];
    char stringpool_str107[sizeof("ISO-IR-58")];
    char stringpool_str108[sizeof("ISO88591")];
    char stringpool_str109[sizeof("ISO8859-9")];
    char stringpool_str110[sizeof("C99")];
    char stringpool_str111[sizeof("ISO8859-1")];
    char stringpool_str112[sizeof("ISO-8859-9")];
    char stringpool_str113[sizeof("ISO_8859-9")];
    char stringpool_str114[sizeof("ISO-8859-1")];
    char stringpool_str115[sizeof("ISO_8859-1")];
    char stringpool_str116[sizeof("ISO8859-11")];
    char stringpool_str117[sizeof("ISO-IR-148")];
    char stringpool_str118[sizeof("L7")];
    char stringpool_str119[sizeof("ISO-8859-11")];
    char stringpool_str120[sizeof("ISO_8859-11")];
    char stringpool_str121[sizeof("CN")];
    char stringpool_str122[sizeof("ISO88596")];
    char stringpool_str124[sizeof("ISO-IR-226")];
    char stringpool_str125[sizeof("ISO8859-6")];
    char stringpool_str128[sizeof("ISO-8859-6")];
    char stringpool_str129[sizeof("ISO_8859-6")];
    char stringpool_str130[sizeof("ISO8859-16")];
    char stringpool_str131[sizeof("L10")];
    char stringpool_str132[sizeof("ISO_8859-16:2001")];
    char stringpool_str133[sizeof("ISO-8859-16")];
    char stringpool_str134[sizeof("ISO_8859-16")];
    char stringpool_str136[sizeof("ISO88595")];
    char stringpool_str138[sizeof("CP949")];
    char stringpool_str139[sizeof("ISO8859-5")];
    char stringpool_str140[sizeof("ISO-IR-179")];
    char stringpool_str141[sizeof("ISO885915")];
    char stringpool_str142[sizeof("ISO-8859-5")];
    char stringpool_str143[sizeof("ISO_8859-5")];
    char stringpool_str144[sizeof("ISO8859-15")];
    char stringpool_str145[sizeof("850")];
    char stringpool_str146[sizeof("CP819")];
    char stringpool_str147[sizeof("ISO-8859-15")];
    char stringpool_str148[sizeof("ISO_8859-15")];
    char stringpool_str149[sizeof("ISO8859-4")];
    char stringpool_str150[sizeof("ISO-IR-110")];
    char stringpool_str151[sizeof("RK1048")];
    char stringpool_str152[sizeof("ISO-8859-4")];
    char stringpool_str153[sizeof("ISO_8859-4")];
    char stringpool_str154[sizeof("ISO8859-14")];
    char stringpool_str156[sizeof("ISO-IR-138")];
    char stringpool_str157[sizeof("ISO-8859-14")];
    char stringpool_str158[sizeof("ISO_8859-14")];
    char stringpool_str160[sizeof("ISO88598")];
    char stringpool_str163[sizeof("ISO8859-8")];
    char stringpool_str165[sizeof("ISO_8859-15:1998")];
    char stringpool_str166[sizeof("ISO-8859-8")];
    char stringpool_str167[sizeof("ISO_8859-8")];
    char stringpool_str169[sizeof("CP866")];
    char stringpool_str170[sizeof("ISO_8859-14:1998")];
    char stringpool_str171[sizeof("CHAR")];
    char stringpool_str172[sizeof("ISO88592")];
    char stringpool_str173[sizeof("CP1251")];
    char stringpool_str174[sizeof("CP154")];
    char stringpool_str175[sizeof("ISO8859-2")];
    char stringpool_str176[sizeof("HZ")];
    char stringpool_str177[sizeof("ASCII")];
    char stringpool_str178[sizeof("ISO-8859-2")];
    char stringpool_str179[sizeof("ISO_8859-2")];
    char stringpool_str182[sizeof("PTCP154")];
    char stringpool_str185[sizeof("CP1131")];
    char stringpool_str187[sizeof("CP1256")];
    char stringpool_str191[sizeof("GBK")];
    char stringpool_str192[sizeof("CP1361")];
    char stringpool_str193[sizeof("CP936")];
    char stringpool_str196[sizeof("KOREAN")];
    char stringpool_str198[sizeof("IBM819")];
    char stringpool_str200[sizeof("EUCKR")];
    char stringpool_str201[sizeof("CP1255")];
    char stringpool_str202[sizeof("ISO-IR-100")];
    char stringpool_str203[sizeof("EUC-KR")];
    char stringpool_str204[sizeof("ISO646-CN")];
    char stringpool_str207[sizeof("KZ-1048")];
    char stringpool_str208[sizeof("CSKOI8R")];
    char stringpool_str209[sizeof("ISO_8859-10:1992")];
    char stringpool_str211[sizeof("CP1254")];
    char stringpool_str212[sizeof("LATIN1")];
    char stringpool_str213[sizeof("LATIN-9")];
    char stringpool_str216[sizeof("US")];
    char stringpool_str219[sizeof("CP862")];
    char stringpool_str220[sizeof("ISO8859-10")];
    char stringpool_str221[sizeof("IBM866")];
    char stringpool_str223[sizeof("ISO-8859-10")];
    char stringpool_str224[sizeof("ISO_8859-10")];
    char stringpool_str225[sizeof("CP1258")];
    char stringpool_str226[sizeof("LATIN6")];
    char stringpool_str227[sizeof("ISO8859-3")];
    char stringpool_str230[sizeof("ISO-8859-3")];
    char stringpool_str231[sizeof("ISO_8859-3")];
    char stringpool_str232[sizeof("ISO8859-13")];
    char stringpool_str233[sizeof("ISO_8859-9:1989")];
    char stringpool_str235[sizeof("ISO-8859-13")];
    char stringpool_str236[sizeof("ISO_8859-13")];
    char stringpool_str237[sizeof("CP1252")];
    char stringpool_str238[sizeof("CHINESE")];
    char stringpool_str239[sizeof("CP950")];
    char stringpool_str240[sizeof("LATIN5")];
    char stringpool_str241[sizeof("MS936")];
    char stringpool_str242[sizeof("ISO_646.IRV:1991")];
    char stringpool_str243[sizeof("CP932")];
    char stringpool_str246[sizeof("ISO-IR-203")];
    char stringpool_str247[sizeof("ISO-IR-57")];
    char stringpool_str248[sizeof("MS-ANSI")];
    char stringpool_str249[sizeof("ISO-2022-KR")];
    char stringpool_str250[sizeof("LATIN4")];
    char stringpool_str252[sizeof("ISO-IR-157")];
    char stringpool_str256[sizeof("VISCII")];
    char stringpool_str258[sizeof("GB2312")];
    char stringpool_str259[sizeof("ISO-IR-87")];
    char stringpool_str260[sizeof("KSC_5601")];
    char stringpool_str261[sizeof("MS-EE")];
    char stringpool_str262[sizeof("CP50221")];
    char stringpool_str263[sizeof("GREEK")];
    char stringpool_str264[sizeof("LATIN8")];
    char stringpool_str266[sizeof("CP850")];
    char stringpool_str267[sizeof("UCS-4")];
    char stringpool_str269[sizeof("GREEK8")];
    char stringpool_str270[sizeof("ISO-IR-127")];
    char stringpool_str271[sizeof("IBM862")];
    char stringpool_str273[sizeof("GEORGIAN-PS")];
    char stringpool_str274[sizeof("KS_C_5601-1989")];
    char stringpool_str275[sizeof("ISO_8859-5:1988")];
    char stringpool_str276[sizeof("LATIN2")];
    char stringpool_str277[sizeof("CP1250")];
    char stringpool_str278[sizeof("EUCCN")];
    char stringpool_str280[sizeof("ISO_8859-4:1988")];
    char stringpool_str281[sizeof("EUC-CN")];
    char stringpool_str282[sizeof("CP874")];
    char stringpool_str284[sizeof("CSASCII")];
    char stringpool_str285[sizeof("ISO646-US")];
    char stringpool_str287[sizeof("ISO_8859-8:1988")];
    char stringpool_str288[sizeof("BIG5")];
    char stringpool_str289[sizeof("CP1253")];
    char stringpool_str291[sizeof("BIG-5")];
    char stringpool_str293[sizeof("UCS-2")];
    char stringpool_str296[sizeof("X0201")];
    char stringpool_str297[sizeof("ROMAN8")];
    char stringpool_str298[sizeof("VISCII1.1-1")];
    char stringpool_str299[sizeof("MS-CYRL")];
    char stringpool_str300[sizeof("ISO88597")];
    char stringpool_str301[sizeof("CP1133")];
    char stringpool_str302[sizeof("GB_1988-80")];
    char stringpool_str303[sizeof("ISO8859-7")];
    char stringpool_str305[sizeof("MULELAO-1")];
    char stringpool_str306[sizeof("ISO-8859-7")];
    char stringpool_str307[sizeof("ISO_8859-7")];
    char stringpool_str308[sizeof("X0212")];
    char stringpool_str310[sizeof("ECMA-114")];
    char stringpool_str312[sizeof("CSKZ1048")];
    char stringpool_str314[sizeof("ELOT_928")];
    char stringpool_str317[sizeof("STRK1048-2002")];
    char stringpool_str318[sizeof("IBM850")];
    char stringpool_str319[sizeof("ISO_8859-3:1988")];
    char stringpool_str321[sizeof("LATIN10")];
    char stringpool_str322[sizeof("UTF-16")];
    char stringpool_str324[sizeof("ECMA-118")];
    char stringpool_str327[sizeof("ISO-2022-CN")];
    char stringpool_str328[sizeof("LATIN3")];
    char stringpool_str330[sizeof("TIS620")];
    char stringpool_str331[sizeof("ISO_8859-1:1987")];
    char stringpool_str332[sizeof("UCS-4LE")];
    char stringpool_str333[sizeof("TIS-620")];
    char stringpool_str334[sizeof("HP15CN")];
    char stringpool_str337[sizeof("TCVN")];
    char stringpool_str338[sizeof("ISO_8859-6:1987")];
    char stringpool_str342[sizeof("CYRILLIC-ASIAN")];
    char stringpool_str345[sizeof("UCS-2LE")];
    char stringpool_str348[sizeof("X0208")];
    char stringpool_str349[sizeof("GB18030")];
    char stringpool_str350[sizeof("CSISO2022KR")];
    char stringpool_str351[sizeof("CYRILLIC")];
    char stringpool_str352[sizeof("UTF8")];
    char stringpool_str354[sizeof("MAC")];
    char stringpool_str355[sizeof("UTF-8")];
    char stringpool_str362[sizeof("CSVISCII")];
    char stringpool_str363[sizeof("ISO_8859-2:1987")];
    char stringpool_str365[sizeof("CP1257")];
    char stringpool_str368[sizeof("ISO-10646-UCS-4")];
    char stringpool_str371[sizeof("KS_C_5601-1987")];
    char stringpool_str373[sizeof("GB_2312-80")];
    char stringpool_str379[sizeof("CP367")];
    char stringpool_str380[sizeof("JP")];
    char stringpool_str381[sizeof("ISO-10646-UCS-2")];
    char stringpool_str387[sizeof("UNICODE-1-1")];
    char stringpool_str389[sizeof("ISO_8859-7:2003")];
    char stringpool_str390[sizeof("TIS620-0")];
    char stringpool_str393[sizeof("ARMSCII-8")];
    char stringpool_str394[sizeof("CSBIG5")];
    char stringpool_str395[sizeof("CSISOLATIN1")];
    char stringpool_str396[sizeof("US-ASCII")];
    char stringpool_str397[sizeof("CN-BIG5")];
    char stringpool_str399[sizeof("UTF-16LE")];
    char stringpool_str400[sizeof("TIS620.2529-1")];
    char stringpool_str401[sizeof("ANSI_X3.4-1986")];
    char stringpool_str404[sizeof("LATIN7")];
    char stringpool_str405[sizeof("CN-GB-ISOIR165")];
    char stringpool_str409[sizeof("CSISOLATIN6")];
    char stringpool_str415[sizeof("UHC")];
    char stringpool_str418[sizeof("KOI8-T")];
    char stringpool_str419[sizeof("MS-GREEK")];
    char stringpool_str420[sizeof("ANSI_X3.4-1968")];
    char stringpool_str423[sizeof("CSISOLATIN5")];
    char stringpool_str425[sizeof("WINDOWS-1251")];
    char stringpool_str427[sizeof("ISO_8859-7:1987")];
    char stringpool_str428[sizeof("CSISO2022CN")];
    char stringpool_str430[sizeof("UTF-32")];
    char stringpool_str431[sizeof("IBM367")];
    char stringpool_str432[sizeof("WINDOWS-1256")];
    char stringpool_str433[sizeof("CSISOLATIN4")];
    char stringpool_str436[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str439[sizeof("WINDOWS-1255")];
    char stringpool_str444[sizeof("WINDOWS-1254")];
    char stringpool_str445[sizeof("ASMO-708")];
    char stringpool_str446[sizeof("CSPTCP154")];
    char stringpool_str448[sizeof("ISO646-JP")];
    char stringpool_str449[sizeof("WINDOWS-936")];
    char stringpool_str451[sizeof("WINDOWS-1258")];
    char stringpool_str457[sizeof("WINDOWS-1252")];
    char stringpool_str459[sizeof("CSISOLATIN2")];
    char stringpool_str462[sizeof("IBM-CP1133")];
    char stringpool_str468[sizeof("CSUCS4")];
    char stringpool_str470[sizeof("CSKSC56011987")];
    char stringpool_str474[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str477[sizeof("WINDOWS-1250")];
    char stringpool_str479[sizeof("HP-ROMAN8")];
    char stringpool_str482[sizeof("UTF-32LE")];
    char stringpool_str483[sizeof("WINDOWS-1253")];
    char stringpool_str485[sizeof("TIS620.2533-1")];
    char stringpool_str486[sizeof("UCS-4BE")];
    char stringpool_str487[sizeof("UCS-4-SWAPPED")];
    char stringpool_str489[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str490[sizeof("UCS-4-INTERNAL")];
    char stringpool_str493[sizeof("TCVN5712-1")];
    char stringpool_str494[sizeof("CSIBM866")];
    char stringpool_str495[sizeof("UTF-7")];
    char stringpool_str499[sizeof("UCS-2BE")];
    char stringpool_str500[sizeof("UCS-2-SWAPPED")];
    char stringpool_str501[sizeof("MACICELAND")];
    char stringpool_str502[sizeof("CSUNICODE11")];
    char stringpool_str503[sizeof("UCS-2-INTERNAL")];
    char stringpool_str505[sizeof("ARABIC")];
    char stringpool_str511[sizeof("CSISOLATIN3")];
    char stringpool_str513[sizeof("CSEUCKR")];
    char stringpool_str515[sizeof("MACROMAN")];
    char stringpool_str516[sizeof("KOI8-U")];
    char stringpool_str518[sizeof("MACGREEK")];
    char stringpool_str519[sizeof("KOI8-RU")];
    char stringpool_str520[sizeof("TCVN-5712")];
    char stringpool_str521[sizeof("WINDOWS-1257")];
    char stringpool_str522[sizeof("EUCJP")];
    char stringpool_str523[sizeof("CSUNICODE")];
    char stringpool_str524[sizeof("CSISOLATINGREEK")];
    char stringpool_str525[sizeof("EUC-JP")];
    char stringpool_str526[sizeof("CN-GB")];
    char stringpool_str531[sizeof("CSGB2312")];
    char stringpool_str532[sizeof("CSISO57GB1988")];
    char stringpool_str536[sizeof("MS-HEBR")];
    char stringpool_str537[sizeof("TIS620.2533-0")];
    char stringpool_str538[sizeof("WINDOWS-874")];
    char stringpool_str541[sizeof("ISO-CELTIC")];
    char stringpool_str542[sizeof("BIGFIVE")];
    char stringpool_str545[sizeof("BIG-FIVE")];
    char stringpool_str553[sizeof("UTF-16BE")];
    char stringpool_str555[sizeof("CSISOLATINARABIC")];
    char stringpool_str571[sizeof("ISO-2022-JP")];
    char stringpool_str574[sizeof("ISO-2022-JP-1")];
    char stringpool_str575[sizeof("ISO-2022-JP-MS")];
    char stringpool_str577[sizeof("GB18030:2022")];
    char stringpool_str579[sizeof("GB18030:2005")];
    char stringpool_str581[sizeof("CSISO58GB231280")];
    char stringpool_str582[sizeof("CSHPROMAN8")];
    char stringpool_str584[sizeof("NEXTSTEP")];
    char stringpool_str594[sizeof("JIS0208")];
    char stringpool_str596[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str599[sizeof("CSISO159JISX02121990")];
    char stringpool_str601[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str602[sizeof("CSUNICODE11UTF7")];
    char stringpool_str605[sizeof("HZ-GB-2312")];
    char stringpool_str606[sizeof("ISO-2022-JP-2")];
    char stringpool_str610[sizeof("MACCYRILLIC")];
    char stringpool_str612[sizeof("BIG5HKSCS")];
    char stringpool_str615[sizeof("BIG5-HKSCS")];
    char stringpool_str618[sizeof("EUCTW")];
    char stringpool_str621[sizeof("EUC-TW")];
    char stringpool_str624[sizeof("MS-ARAB")];
    char stringpool_str626[sizeof("MACROMANIA")];
    char stringpool_str636[sizeof("UTF-32BE")];
    char stringpool_str639[sizeof("JAVA")];
    char stringpool_str641[sizeof("MS-TURK")];
    char stringpool_str643[sizeof("SHIFT-JIS")];
    char stringpool_str644[sizeof("SHIFT_JIS")];
    char stringpool_str651[sizeof("CSISO14JISC6220RO")];
    char stringpool_str653[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str655[sizeof("MACTHAI")];
    char stringpool_str659[sizeof("JIS_C6226-1983")];
    char stringpool_str671[sizeof("TCVN5712-1:1993")];
    char stringpool_str672[sizeof("CSISO2022JP")];
    char stringpool_str676[sizeof("MS_KANJI")];
    char stringpool_str680[sizeof("MACUKRAINE")];
    char stringpool_str696[sizeof("MACCROATIAN")];
    char stringpool_str698[sizeof("HEBREW")];
    char stringpool_str699[sizeof("UNICODEBIG")];
    char stringpool_str700[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str706[sizeof("CSISO2022JP2")];
    char stringpool_str719[sizeof("CSISOLATINHEBREW")];
    char stringpool_str723[sizeof("WCHAR_T")];
    char stringpool_str731[sizeof("MACCENTRALEUROPE")];
    char stringpool_str734[sizeof("BIG5-HKSCS:1999")];
    char stringpool_str735[sizeof("BIG5-HKSCS:2001")];
    char stringpool_str738[sizeof("JIS_X0201")];
    char stringpool_str749[sizeof("UNICODELITTLE")];
    char stringpool_str750[sizeof("JIS_X0212")];
    char stringpool_str754[sizeof("BIG5-HKSCS:2004")];
    char stringpool_str755[sizeof("JISX0201-1976")];
    char stringpool_str760[sizeof("MACARABIC")];
    char stringpool_str761[sizeof("BIG5-HKSCS:2008")];
    char stringpool_str781[sizeof("JIS_X0212-1990")];
    char stringpool_str790[sizeof("JIS_X0208")];
    char stringpool_str791[sizeof("MACINTOSH")];
    char stringpool_str803[sizeof("JIS_X0212.1990-0")];
    char stringpool_str827[sizeof("JIS_X0208-1990")];
    char stringpool_str833[sizeof("JIS_X0208-1983")];
    char stringpool_str864[sizeof("CSISO87JISX0208")];
    char stringpool_str880[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str898[sizeof("WINBALTRIM")];
    char stringpool_str912[sizeof("CSSHIFTJIS")];
    char stringpool_str931[sizeof("CSEUCTW")];
    char stringpool_str950[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str951[sizeof("JOHAB")];
    char stringpool_str956[sizeof("CSMACINTOSH")];
    char stringpool_str986[sizeof("MACHEBREW")];
    char stringpool_str1001[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str1033[sizeof("MACTURKISH")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "SJIS",
    "L1",
    "L6",
    "R8",
    "L5",
    "L4",
    "ISO-IR-199",
    "L8",
    "ISO-IR-6",
    "L2",
    "866",
    "ISO-IR-159",
    "ISO-IR-149",
    "ISO-IR-166",
    "PT154",
    "ISO-IR-14",
    "L3",
    "ISO-IR-165",
    "ISO-IR-126",
    "ISO-IR-109",
    "ISO-IR-101",
    "KOI8-R",
    "ISO-IR-144",
    "862",
    "ISO88599",
    "ISO-IR-58",
    "ISO88591",
    "ISO8859-9",
    "C99",
    "ISO8859-1",
    "ISO-8859-9",
    "ISO_8859-9",
    "ISO-8859-1",
    "ISO_8859-1",
    "ISO8859-11",
    "ISO-IR-148",
    "L7",
    "ISO-8859-11",
    "ISO_8859-11",
    "CN",
    "ISO88596",
    "ISO-IR-226",
    "ISO8859-6",
    "ISO-8859-6",
    "ISO_8859-6",
    "ISO8859-16",
    "L10",
    "ISO_8859-16:2001",
    "ISO-8859-16",
    "ISO_8859-16",
    "ISO88595",
    "CP949",
    "ISO8859-5",
    "ISO-IR-179",
    "ISO885915",
    "ISO-8859-5",
    "ISO_8859-5",
    "ISO8859-15",
    "850",
    "CP819",
    "ISO-8859-15",
    "ISO_8859-15",
    "ISO8859-4",
    "ISO-IR-110",
    "RK1048",
    "ISO-8859-4",
    "ISO_8859-4",
    "ISO8859-14",
    "ISO-IR-138",
    "ISO-8859-14",
    "ISO_8859-14",
    "ISO88598",
    "ISO8859-8",
    "ISO_8859-15:1998",
    "ISO-8859-8",
    "ISO_8859-8",
    "CP866",
    "ISO_8859-14:1998",
    "CHAR",
    "ISO88592",
    "CP1251",
    "CP154",
    "ISO8859-2",
    "HZ",
    "ASCII",
    "ISO-8859-2",
    "ISO_8859-2",
    "PTCP154",
    "CP1131",
    "CP1256",
    "GBK",
    "CP1361",
    "CP936",
    "KOREAN",
    "IBM819",
    "EUCKR",
    "CP1255",
    "ISO-IR-100",
    "EUC-KR",
    "ISO646-CN",
    "KZ-1048",
    "CSKOI8R",
    "ISO_8859-10:1992",
    "CP1254",
    "LATIN1",
    "LATIN-9",
    "US",
    "CP862",
    "ISO8859-10",
    "IBM866",
    "ISO-8859-10",
    "ISO_8859-10",
    "CP1258",
    "LATIN6",
    "ISO8859-3",
    "ISO-8859-3",
    "ISO_8859-3",
    "ISO8859-13",
    "ISO_8859-9:1989",
    "ISO-8859-13",
    "ISO_8859-13",
    "CP1252",
    "CHINESE",
    "CP950",
    "LATIN5",
    "MS936",
    "ISO_646.IRV:1991",
    "CP932",
    "ISO-IR-203",
    "ISO-IR-57",
    "MS-ANSI",
    "ISO-2022-KR",
    "LATIN4",
    "ISO-IR-157",
    "VISCII",
    "GB2312",
    "ISO-IR-87",
    "KSC_5601",
    "MS-EE",
    "CP50221",
    "GREEK",
    "LATIN8",
    "CP850",
    "UCS-4",
    "GREEK8",
    "ISO-IR-127",
    "IBM862",
    "GEORGIAN-PS",
    "KS_C_5601-1989",
    "ISO_8859-5:1988",
    "LATIN2",
    "CP1250",
    "EUCCN",
    "ISO_8859-4:1988",
    "EUC-CN",
    "CP874",
    "CSASCII",
    "ISO646-US",
    "ISO_8859-8:1988",
    "BIG5",
    "CP1253",
    "BIG-5",
    "UCS-2",
    "X0201",
    "ROMAN8",
    "VISCII1.1-1",
    "MS-CYRL",
    "ISO88597",
    "CP1133",
    "GB_1988-80",
    "ISO8859-7",
    "MULELAO-1",
    "ISO-8859-7",
    "ISO_8859-7",
    "X0212",
    "ECMA-114",
    "CSKZ1048",
    "ELOT_928",
    "STRK1048-2002",
    "IBM850",
    "ISO_8859-3:1988",
    "LATIN10",
    "UTF-16",
    "ECMA-118",
    "ISO-2022-CN",
    "LATIN3",
    "TIS620",
    "ISO_8859-1:1987",
    "UCS-4LE",
    "TIS-620",
    "HP15CN",
    "TCVN",
    "ISO_8859-6:1987",
    "CYRILLIC-ASIAN",
    "UCS-2LE",
    "X0208",
    "GB18030",
    "CSISO2022KR",
    "CYRILLIC",
    "UTF8",
    "MAC",
    "UTF-8",
    "CSVISCII",
    "ISO_8859-2:1987",
    "CP1257",
    "ISO-10646-UCS-4",
    "KS_C_5601-1987",
    "GB_2312-80",
    "CP367",
    "JP",
    "ISO-10646-UCS-2",
    "UNICODE-1-1",
    "ISO_8859-7:2003",
    "TIS620-0",
    "ARMSCII-8",
    "CSBIG5",
    "CSISOLATIN1",
    "US-ASCII",
    "CN-BIG5",
    "UTF-16LE",
    "TIS620.2529-1",
    "ANSI_X3.4-1986",
    "LATIN7",
    "CN-GB-ISOIR165",
    "CSISOLATIN6",
    "UHC",
    "KOI8-T",
    "MS-GREEK",
    "ANSI_X3.4-1968",
    "CSISOLATIN5",
    "WINDOWS-1251",
    "ISO_8859-7:1987",
    "CSISO2022CN",
    "UTF-32",
    "IBM367",
    "WINDOWS-1256",
    "CSISOLATIN4",
    "GEORGIAN-ACADEMY",
    "WINDOWS-1255",
    "WINDOWS-1254",
    "ASMO-708",
    "CSPTCP154",
    "ISO646-JP",
    "WINDOWS-936",
    "WINDOWS-1258",
    "WINDOWS-1252",
    "CSISOLATIN2",
    "IBM-CP1133",
    "CSUCS4",
    "CSKSC56011987",
    "ISO-2022-CN-EXT",
    "WINDOWS-1250",
    "HP-ROMAN8",
    "UTF-32LE",
    "WINDOWS-1253",
    "TIS620.2533-1",
    "UCS-4BE",
    "UCS-4-SWAPPED",
    "UNICODE-1-1-UTF-7",
    "UCS-4-INTERNAL",
    "TCVN5712-1",
    "CSIBM866",
    "UTF-7",
    "UCS-2BE",
    "UCS-2-SWAPPED",
    "MACICELAND",
    "CSUNICODE11",
    "UCS-2-INTERNAL",
    "ARABIC",
    "CSISOLATIN3",
    "CSEUCKR",
    "MACROMAN",
    "KOI8-U",
    "MACGREEK",
    "KOI8-RU",
    "TCVN-5712",
    "WINDOWS-1257",
    "EUCJP",
    "CSUNICODE",
    "CSISOLATINGREEK",
    "EUC-JP",
    "CN-GB",
    "CSGB2312",
    "CSISO57GB1988",
    "MS-HEBR",
    "TIS620.2533-0",
    "WINDOWS-874",
    "ISO-CELTIC",
    "BIGFIVE",
    "BIG-FIVE",
    "UTF-16BE",
    "CSISOLATINARABIC",
    "ISO-2022-JP",
    "ISO-2022-JP-1",
    "ISO-2022-JP-MS",
    "GB18030:2022",
    "GB18030:2005",
    "CSISO58GB231280",
    "CSHPROMAN8",
    "NEXTSTEP",
    "JIS0208",
    "CSISOLATINCYRILLIC",
    "CSISO159JISX02121990",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "CSUNICODE11UTF7",
    "HZ-GB-2312",
    "ISO-2022-JP-2",
    "MACCYRILLIC",
    "BIG5HKSCS",
    "BIG5-HKSCS",
    "EUCTW",
    "EUC-TW",
    "MS-ARAB",
    "MACROMANIA",
    "UTF-32BE",
    "JAVA",
    "MS-TURK",
    "SHIFT-JIS",
    "SHIFT_JIS",
    "CSISO14JISC6220RO",
    "JIS_C6220-1969-RO",
    "MACTHAI",
    "JIS_C6226-1983",
    "TCVN5712-1:1993",
    "CSISO2022JP",
    "MS_KANJI",
    "MACUKRAINE",
    "MACCROATIAN",
    "HEBREW",
    "UNICODEBIG",
    "CSPC862LATINHEBREW",
    "CSISO2022JP2",
    "CSISOLATINHEBREW",
    "WCHAR_T",
    "MACCENTRALEUROPE",
    "BIG5-HKSCS:1999",
    "BIG5-HKSCS:2001",
    "JIS_X0201",
    "UNICODELITTLE",
    "JIS_X0212",
    "BIG5-HKSCS:2004",
    "JISX0201-1976",
    "MACARABIC",
    "BIG5-HKSCS:2008",
    "JIS_X0212-1990",
    "JIS_X0208",
    "MACINTOSH",
    "JIS_X0212.1990-0",
    "JIS_X0208-1990",
    "JIS_X0208-1983",
    "CSISO87JISX0208",
    "CSPC850MULTILINGUAL",
    "WINBALTRIM",
    "CSSHIFTJIS",
    "CSEUCTW",
    "CSEUCPKDFMTJAPANESE",
    "JOHAB",
    "CSMACINTOSH",
    "MACHEBREW",
    "CSHALFWIDTHKATAKANA",
    "MACTURKISH"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 317 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str19, ei_sjis},
    {-1}, {-1},
#line 61 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22, ei_iso8859_1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 142 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29, ei_iso8859_10},
    {-1}, {-1}, {-1}, {-1},
#line 236 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34, ei_hp_roman8},
    {-1},
#line 133 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36, ei_iso8859_9},
    {-1}, {-1}, {-1}, {-1},
#line 87 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str41, ei_iso8859_4},
    {-1},
#line 157 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43, ei_iso8859_14},
    {-1}, {-1}, {-1}, {-1},
#line 159 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48, ei_iso8859_14},
    {-1},
#line 16 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str50, ei_ascii},
    {-1}, {-1}, {-1},
#line 70 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54, ei_iso8859_2},
#line 216 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55, ei_cp866},
    {-1}, {-1},
#line 292 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str58, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1},
#line 308 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str63, ei_ksc5601},
    {-1}, {-1}, {-1},
#line 261 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67, ei_tis620},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 243 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str76, ei_pt154},
    {-1}, {-1},
#line 273 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79, ei_iso646_jp},
#line 79 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80, ei_iso8859_3},
#line 303 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str81, ei_isoir165},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 112 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92, ei_iso8859_7},
    {-1}, {-1}, {-1},
#line 77 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96, ei_iso8859_3},
    {-1},
#line 68 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str98, ei_iso8859_2},
    {-1},
#line 176 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str100, ei_koi8_r},
    {-1}, {-1},
#line 93 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103, ei_iso8859_5},
    {-1},
#line 212 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105, ei_cp862},
#line 136 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106, ei_iso8859_9},
#line 300 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str107, ei_gb2312},
#line 64 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108, ei_iso8859_1},
#line 135 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str109, ei_iso8859_9},
#line 52 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110, ei_c99},
#line 63 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111, ei_iso8859_1},
#line 128 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112, ei_iso8859_9},
#line 129 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str113, ei_iso8859_9},
#line 54 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str114, ei_iso8859_1},
#line 55 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115, ei_iso8859_1},
#line 147 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str116, ei_iso8859_11},
#line 131 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117, ei_iso8859_9},
#line 152 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str118, ei_iso8859_13},
#line 145 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119, ei_iso8859_11},
#line 146 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str120, ei_iso8859_11},
#line 297 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121, ei_iso646_cn},
#line 107 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str122, ei_iso8859_6},
    {-1},
#line 172 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124, ei_iso8859_16},
#line 106 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125, ei_iso8859_6},
    {-1}, {-1},
#line 98 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128, ei_iso8859_6},
#line 99 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129, ei_iso8859_6},
#line 175 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130, ei_iso8859_16},
#line 174 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131, ei_iso8859_16},
#line 171 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str132, ei_iso8859_16},
#line 169 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str133, ei_iso8859_16},
#line 170 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str134, ei_iso8859_16},
    {-1},
#line 97 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136, ei_iso8859_5},
    {-1},
#line 365 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138, ei_cp949},
#line 96 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139, ei_iso8859_5},
#line 150 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140, ei_iso8859_13},
#line 168 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str141, ei_iso8859_15},
#line 90 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142, ei_iso8859_5},
#line 91 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143, ei_iso8859_5},
#line 167 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str144, ei_iso8859_15},
#line 208 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145, ei_cp850},
#line 58 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146, ei_iso8859_1},
#line 162 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147, ei_iso8859_15},
#line 163 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148, ei_iso8859_15},
#line 89 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149, ei_iso8859_4},
#line 85 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150, ei_iso8859_4},
#line 248 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151, ei_rk1048},
#line 82 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152, ei_iso8859_4},
#line 83 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153, ei_iso8859_4},
#line 161 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154, ei_iso8859_14},
    {-1},
#line 123 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156, ei_iso8859_8},
#line 154 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157, ei_iso8859_14},
#line 155 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158, ei_iso8859_14},
    {-1},
#line 127 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160, ei_iso8859_8},
    {-1}, {-1},
#line 126 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163, ei_iso8859_8},
    {-1},
#line 164 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str165, ei_iso8859_15},
#line 120 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166, ei_iso8859_8},
#line 121 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167, ei_iso8859_8},
    {-1},
#line 214 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str169, ei_cp866},
#line 156 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170, ei_iso8859_14},
#line 371 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171, ei_local_char},
#line 73 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172, ei_iso8859_2},
#line 183 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173, ei_cp1251},
#line 245 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174, ei_pt154},
#line 72 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str175, ei_iso8859_2},
#line 344 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176, ei_hz},
#line 13 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177, ei_ascii},
#line 65 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str178, ei_iso8859_2},
#line 66 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_2},
    {-1}, {-1},
#line 244 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182, ei_pt154},
    {-1}, {-1},
#line 218 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str185, ei_cp1131},
    {-1},
#line 198 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187, ei_cp1256},
    {-1}, {-1}, {-1},
#line 334 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191, ei_ces_gbk},
#line 368 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192, ei_johab},
#line 335 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193, ei_cp936},
    {-1}, {-1},
#line 310 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196, ei_ksc5601},
    {-1},
#line 59 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198, ei_iso8859_1},
    {-1},
#line 363 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str200, ei_euc_kr},
#line 195 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201, ei_cp1255},
#line 57 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202, ei_iso8859_1},
#line 362 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str203, ei_euc_kr},
#line 295 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str204, ei_iso646_cn},
    {-1}, {-1},
#line 250 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207, ei_rk1048},
#line 177 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str208, ei_koi8_r},
#line 139 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209, ei_iso8859_10},
    {-1},
#line 192 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211, ei_cp1254},
#line 60 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212, ei_iso8859_1},
#line 166 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str213, ei_iso8859_15},
    {-1}, {-1},
#line 21 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216, ei_ascii},
    {-1}, {-1},
#line 210 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219, ei_cp862},
#line 144 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220, ei_iso8859_10},
#line 215 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str221, ei_cp866},
    {-1},
#line 137 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223, ei_iso8859_10},
#line 138 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str224, ei_iso8859_10},
#line 204 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225, ei_cp1258},
#line 141 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str226, ei_iso8859_10},
#line 81 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227, ei_iso8859_3},
    {-1}, {-1},
#line 74 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str230, ei_iso8859_3},
#line 75 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231, ei_iso8859_3},
#line 153 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str232, ei_iso8859_13},
#line 130 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str233, ei_iso8859_9},
    {-1},
#line 148 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235, ei_iso8859_13},
#line 149 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236, ei_iso8859_13},
#line 186 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237, ei_cp1252},
#line 302 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238, ei_gb2312},
#line 355 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239, ei_cp950},
#line 132 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240, ei_iso8859_9},
#line 336 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241, ei_cp936},
#line 15 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242, ei_ascii},
#line 320 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243, ei_cp932},
    {-1}, {-1},
#line 165 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246, ei_iso8859_15},
#line 296 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str247, ei_iso646_cn},
#line 188 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248, ei_cp1252},
#line 369 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249, ei_iso2022_kr},
#line 86 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str250, ei_iso8859_4},
    {-1},
#line 140 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str252, ei_iso8859_10},
    {-1}, {-1}, {-1},
#line 264 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str256, ei_viscii},
    {-1},
#line 330 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258, ei_euc_cn},
#line 285 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259, ei_jisx0208},
#line 305 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260, ei_ksc5601},
#line 182 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261, ei_cp1250},
#line 327 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str262, ei_iso2022_jpms},
#line 116 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str263, ei_iso8859_7},
#line 158 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264, ei_iso8859_14},
    {-1},
#line 206 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str266, ei_cp850},
#line 34 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267, ei_ucs4},
    {-1},
#line 115 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str269, ei_iso8859_7},
#line 101 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270, ei_iso8859_6},
#line 211 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271, ei_cp862},
    {-1},
#line 241 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str273, ei_georgian_ps},
#line 307 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274, ei_ksc5601},
#line 92 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275, ei_iso8859_5},
#line 69 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276, ei_iso8859_2},
#line 180 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str277, ei_cp1250},
#line 329 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278, ei_euc_cn},
    {-1},
#line 84 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280, ei_iso8859_4},
#line 328 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281, ei_euc_cn},
#line 262 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282, ei_cp874},
    {-1},
#line 22 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str284, ei_ascii},
#line 14 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285, ei_ascii},
    {-1},
#line 122 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287, ei_iso8859_8},
#line 349 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288, ei_ces_big5},
#line 189 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str289, ei_cp1253},
    {-1},
#line 350 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291, ei_ces_big5},
    {-1},
#line 25 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str293, ei_ucs2},
    {-1}, {-1},
#line 278 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296, ei_jisx0201},
#line 235 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str297, ei_hp_roman8},
#line 265 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str298, ei_viscii},
#line 185 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str299, ei_cp1251},
#line 119 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str300, ei_iso8859_7},
#line 253 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301, ei_cp1133},
#line 294 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302, ei_iso646_cn},
#line 118 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str303, ei_iso8859_7},
    {-1},
#line 252 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str305, ei_mulelao},
#line 108 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306, ei_iso8859_7},
#line 109 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307, ei_iso8859_7},
#line 291 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str308, ei_jisx0212},
    {-1},
#line 102 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str310, ei_iso8859_6},
    {-1},
#line 251 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312, ei_rk1048},
    {-1},
#line 114 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str314, ei_iso8859_7},
    {-1}, {-1},
#line 249 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str317, ei_rk1048},
#line 207 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318, ei_cp850},
#line 76 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str319, ei_iso8859_3},
    {-1},
#line 173 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321, ei_iso8859_16},
#line 39 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322, ei_utf16},
    {-1},
#line 113 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str324, ei_iso8859_7},
    {-1}, {-1},
#line 341 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327, ei_iso2022_cn},
#line 78 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str328, ei_iso8859_3},
    {-1},
#line 256 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str330, ei_tis620},
#line 56 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str331, ei_iso8859_1},
#line 38 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332, ei_ucs4le},
#line 255 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str333, ei_tis620},
#line 333 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str334, ei_euc_cn},
    {-1}, {-1},
#line 267 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337, ei_tcvn},
#line 100 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str338, ei_iso8859_6},
    {-1}, {-1}, {-1},
#line 246 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str342, ei_pt154},
    {-1}, {-1},
#line 32 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str345, ei_ucs2le},
    {-1}, {-1},
#line 284 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str348, ei_jisx0208},
#line 338 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349, ei_gb18030_2005},
#line 370 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str350, ei_iso2022_kr},
#line 94 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str351, ei_iso8859_5},
#line 24 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352, ei_utf8},
    {-1},
#line 221 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354, ei_mac_roman},
#line 23 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355, ei_utf8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 266 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362, ei_viscii},
#line 67 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str363, ei_iso8859_2},
    {-1},
#line 201 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365, ei_cp1257},
    {-1}, {-1},
#line 35 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str368, ei_ucs4},
    {-1}, {-1},
#line 306 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str371, ei_ksc5601},
    {-1},
#line 299 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373, ei_gb2312},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 19 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str379, ei_ascii},
#line 274 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str380, ei_iso646_jp},
#line 26 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str381, ei_ucs2},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 30 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str387, ei_ucs2be},
    {-1},
#line 111 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str389, ei_iso8859_7},
#line 257 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str390, ei_tis620},
    {-1}, {-1},
#line 239 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str393, ei_armscii_8},
#line 354 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str394, ei_ces_big5},
#line 62 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str395, ei_iso8859_1},
#line 12 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str396, ei_ascii},
#line 353 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str397, ei_ces_big5},
    {-1},
#line 41 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str399, ei_utf16le},
#line 258 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str400, ei_tis620},
#line 18 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str401, ei_ascii},
    {-1}, {-1},
#line 151 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404, ei_iso8859_13},
#line 304 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405, ei_isoir165},
    {-1}, {-1}, {-1},
#line 143 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str409, ei_iso8859_10},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 366 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str415, ei_cp949},
    {-1}, {-1},
#line 242 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418, ei_koi8_t},
#line 191 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str419, ei_cp1253},
#line 17 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str420, ei_ascii},
    {-1}, {-1},
#line 134 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str423, ei_iso8859_9},
    {-1},
#line 184 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425, ei_cp1251},
    {-1},
#line 110 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str427, ei_iso8859_7},
#line 342 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str428, ei_iso2022_cn},
    {-1},
#line 42 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430, ei_utf32},
#line 20 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431, ei_ascii},
#line 199 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str432, ei_cp1256},
#line 88 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433, ei_iso8859_4},
    {-1}, {-1},
#line 240 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str436, ei_georgian_academy},
    {-1}, {-1},
#line 196 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str439, ei_cp1255},
    {-1}, {-1}, {-1}, {-1},
#line 193 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str444, ei_cp1254},
#line 103 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445, ei_iso8859_6},
#line 247 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446, ei_pt154},
    {-1},
#line 272 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448, ei_iso646_jp},
#line 337 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str449, ei_cp936},
    {-1},
#line 205 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str451, ei_cp1258},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 187 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457, ei_cp1252},
    {-1},
#line 71 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459, ei_iso8859_2},
    {-1}, {-1},
#line 254 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str462, ei_cp1133},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 36 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468, ei_ucs4},
    {-1},
#line 309 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470, ei_ksc5601},
    {-1}, {-1}, {-1},
#line 343 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474, ei_iso2022_cn_ext},
    {-1}, {-1},
#line 181 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477, ei_cp1250},
    {-1},
#line 234 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str479, ei_hp_roman8},
    {-1}, {-1},
#line 44 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str482, ei_utf32le},
#line 190 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str483, ei_cp1253},
    {-1},
#line 260 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str485, ei_tis620},
#line 37 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str486, ei_ucs4be},
#line 51 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str487, ei_ucs4swapped},
    {-1},
#line 46 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str489, ei_utf7},
#line 50 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490, ei_ucs4internal},
    {-1}, {-1},
#line 269 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str493, ei_tcvn},
#line 217 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str494, ei_cp866},
#line 45 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495, ei_utf7},
    {-1}, {-1}, {-1},
#line 28 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str499, ei_ucs2be},
#line 49 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str500, ei_ucs2swapped},
#line 224 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str501, ei_mac_iceland},
#line 31 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str502, ei_ucs2be},
#line 48 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str503, ei_ucs2internal},
    {-1},
#line 104 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str505, ei_iso8859_6},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 80 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str511, ei_iso8859_3},
    {-1},
#line 364 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str513, ei_euc_kr},
    {-1},
#line 219 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str515, ei_mac_roman},
#line 178 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str516, ei_koi8_u},
    {-1},
#line 229 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str518, ei_mac_greek},
#line 179 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str519, ei_koi8_ru},
#line 268 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str520, ei_tcvn},
#line 202 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str521, ei_cp1257},
#line 312 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str522, ei_euc_jp},
#line 27 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str523, ei_ucs2},
#line 117 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str524, ei_iso8859_7},
#line 311 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str525, ei_euc_jp},
#line 331 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str526, ei_euc_cn},
    {-1}, {-1}, {-1}, {-1},
#line 332 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str531, ei_euc_cn},
#line 298 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str532, ei_iso646_cn},
    {-1}, {-1}, {-1},
#line 197 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str536, ei_cp1255},
#line 259 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str537, ei_tis620},
#line 263 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str538, ei_cp874},
    {-1}, {-1},
#line 160 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str541, ei_iso8859_14},
#line 352 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str542, ei_ces_big5},
    {-1}, {-1},
#line 351 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str545, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 40 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str553, ei_utf16be},
    {-1},
#line 105 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str555, ei_iso8859_6},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 321 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str571, ei_iso2022_jp},
    {-1}, {-1},
#line 323 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str574, ei_iso2022_jp1},
#line 326 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str575, ei_iso2022_jpms},
    {-1},
#line 340 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str577, ei_gb18030_2022},
    {-1},
#line 339 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str579, ei_gb18030_2005},
    {-1},
#line 301 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str581, ei_gb2312},
#line 237 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str582, ei_hp_roman8},
    {-1},
#line 238 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str584, ei_nextstep},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 283 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str594, ei_jisx0208},
    {-1},
#line 95 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str596, ei_iso8859_5},
    {-1}, {-1},
#line 293 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str599, ei_jisx0212},
    {-1},
#line 313 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str601, ei_euc_jp},
#line 47 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str602, ei_utf7},
    {-1}, {-1},
#line 345 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str605, ei_hz},
#line 324 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str606, ei_iso2022_jp2},
    {-1}, {-1}, {-1},
#line 227 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str610, ei_mac_cyrillic},
    {-1},
#line 360 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str612, ei_big5hkscs2008},
    {-1}, {-1},
#line 359 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str615, ei_big5hkscs2008},
    {-1}, {-1},
#line 347 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str618, ei_euc_tw},
    {-1}, {-1},
#line 346 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str621, ei_euc_tw},
    {-1}, {-1},
#line 200 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str624, ei_cp1256},
    {-1},
#line 226 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str626, ei_mac_romania},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 43 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str636, ei_utf32be},
    {-1}, {-1},
#line 53 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str639, ei_java},
    {-1},
#line 194 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str641, ei_cp1254},
    {-1},
#line 316 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str643, ei_sjis},
#line 315 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str644, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 275 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str651, ei_iso646_jp},
    {-1},
#line 271 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str653, ei_iso646_jp},
    {-1},
#line 233 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str655, ei_mac_thai},
    {-1}, {-1}, {-1},
#line 286 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str659, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 270 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str671, ei_tcvn},
#line 322 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str672, ei_iso2022_jp},
    {-1}, {-1}, {-1},
#line 318 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str676, ei_sjis},
    {-1}, {-1}, {-1},
#line 228 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str680, ei_mac_ukraine},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 225 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str696, ei_mac_croatian},
    {-1},
#line 124 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str698, ei_iso8859_8},
#line 29 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str699, ei_ucs2be},
#line 213 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str700, ei_cp862},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 325 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str706, ei_iso2022_jp2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 125 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str719, ei_iso8859_8},
    {-1}, {-1}, {-1},
#line 372 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str723, ei_local_wchar_t},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 223 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str731, ei_mac_centraleurope},
    {-1}, {-1},
#line 356 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str734, ei_big5hkscs1999},
#line 357 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str735, ei_big5hkscs2001},
    {-1}, {-1},
#line 276 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str738, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 33 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str749, ei_ucs2le},
#line 288 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str750, ei_jisx0212},
    {-1}, {-1}, {-1},
#line 358 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str754, ei_big5hkscs2004},
#line 277 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str755, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1},
#line 232 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str760, ei_mac_arabic},
#line 361 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str761, ei_big5hkscs2008},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 290 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str781, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 280 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str790, ei_jisx0208},
#line 220 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str791, ei_mac_roman},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 289 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str803, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 282 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str827, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 281 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str833, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 287 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str864, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 209 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str880, ei_cp850},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 203 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str898, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 319 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str912, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 348 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str931, ei_euc_tw},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 314 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str950, ei_euc_jp},
#line 367 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str951, ei_johab},
    {-1}, {-1}, {-1}, {-1},
#line 222 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str956, ei_mac_roman},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 231 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str986, ei_mac_hebrew},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 279 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1001, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 230 "lib/aliases_syshpux.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1033, ei_mac_turkish}
  };

static const struct alias *
aliases_lookup (register const char *str, register size_t len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = aliases_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int o = aliases[key].name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &aliases[key];
            }
        }
    }
  return 0;
}
