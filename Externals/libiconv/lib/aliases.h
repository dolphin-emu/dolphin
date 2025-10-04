/* ANSI-C code produced by gperf version 3.2 */
/* Command-line: gperf -m 10 lib/aliases.gperf  */
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

#line 1 "lib/aliases.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 351
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 15
#define MAX_HASH_VALUE 921
/* maximum key range = 907, duplicates = 0 */

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
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922,   5,   6, 922,  97,   7,
       33, 102,  15,  17,   5, 173,  23,  25, 193, 922,
      922, 922, 922, 922, 922, 168, 159,  11,  32,  25,
      159,  13,  70,   6, 308, 186,   8, 132,   7,   6,
       62, 922,   5,   8,  30, 174, 163, 180,  79,  28,
        6, 922, 922, 922, 922,   6, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922, 922, 922,
      922, 922, 922, 922, 922, 922, 922, 922
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
    char stringpool_str15[sizeof("L6")];
    char stringpool_str17[sizeof("L1")];
    char stringpool_str20[sizeof("CN")];
    char stringpool_str25[sizeof("L4")];
    char stringpool_str27[sizeof("L5")];
    char stringpool_str30[sizeof("R8")];
    char stringpool_str33[sizeof("L8")];
    char stringpool_str34[sizeof("SJIS")];
    char stringpool_str36[sizeof("866")];
    char stringpool_str43[sizeof("L2")];
    char stringpool_str51[sizeof("ISO-IR-6")];
    char stringpool_str54[sizeof("CP866")];
    char stringpool_str64[sizeof("C99")];
    char stringpool_str65[sizeof("ISO-IR-166")];
    char stringpool_str66[sizeof("EUCCN")];
    char stringpool_str67[sizeof("LATIN6")];
    char stringpool_str70[sizeof("CP154")];
    char stringpool_str71[sizeof("LATIN1")];
    char stringpool_str72[sizeof("EUC-CN")];
    char stringpool_str74[sizeof("CYRILLIC")];
    char stringpool_str76[sizeof("ISO646-CN")];
    char stringpool_str78[sizeof("HZ")];
    char stringpool_str79[sizeof("ISO-IR-14")];
    char stringpool_str84[sizeof("CP1256")];
    char stringpool_str87[sizeof("LATIN4")];
    char stringpool_str88[sizeof("CP1251")];
    char stringpool_str89[sizeof("ISO-IR-165")];
    char stringpool_str91[sizeof("LATIN5")];
    char stringpool_str92[sizeof("862")];
    char stringpool_str93[sizeof("ISO-IR-126")];
    char stringpool_str95[sizeof("ISO-IR-144")];
    char stringpool_str96[sizeof("CP819")];
    char stringpool_str103[sizeof("LATIN8")];
    char stringpool_str104[sizeof("CP1254")];
    char stringpool_str105[sizeof("ISO-IR-58")];
    char stringpool_str106[sizeof("CP949")];
    char stringpool_str108[sizeof("CP1255")];
    char stringpool_str110[sizeof("CP862")];
    char stringpool_str111[sizeof("ISO-IR-148")];
    char stringpool_str112[sizeof("L3")];
    char stringpool_str113[sizeof("LATIN-9")];
    char stringpool_str114[sizeof("CHINESE")];
    char stringpool_str115[sizeof("ISO-IR-149")];
    char stringpool_str117[sizeof("ISO-IR-159")];
    char stringpool_str119[sizeof("ISO-IR-226")];
    char stringpool_str120[sizeof("CP1258")];
    char stringpool_str121[sizeof("PT154")];
    char stringpool_str123[sizeof("LATIN2")];
    char stringpool_str124[sizeof("ISO8859-6")];
    char stringpool_str125[sizeof("ISO-IR-199")];
    char stringpool_str128[sizeof("ISO8859-1")];
    char stringpool_str129[sizeof("ISO-CELTIC")];
    char stringpool_str130[sizeof("ISO-8859-6")];
    char stringpool_str131[sizeof("ISO_8859-6")];
    char stringpool_str132[sizeof("ISO8859-16")];
    char stringpool_str134[sizeof("ISO-8859-1")];
    char stringpool_str135[sizeof("ISO_8859-1")];
    char stringpool_str136[sizeof("ISO8859-11")];
    char stringpool_str138[sizeof("ISO-8859-16")];
    char stringpool_str139[sizeof("ISO_8859-16")];
    char stringpool_str140[sizeof("CP1252")];
    char stringpool_str142[sizeof("ISO-8859-11")];
    char stringpool_str143[sizeof("ISO_8859-11")];
    char stringpool_str144[sizeof("ISO8859-4")];
    char stringpool_str145[sizeof("CP1361")];
    char stringpool_str146[sizeof("ISO_8859-16:2001")];
    char stringpool_str147[sizeof("CP1131")];
    char stringpool_str148[sizeof("ISO8859-5")];
    char stringpool_str150[sizeof("ISO-8859-4")];
    char stringpool_str151[sizeof("ISO_8859-4")];
    char stringpool_str152[sizeof("ISO8859-14")];
    char stringpool_str153[sizeof("CP936")];
    char stringpool_str154[sizeof("ISO-8859-5")];
    char stringpool_str155[sizeof("ISO_8859-5")];
    char stringpool_str156[sizeof("ISO8859-15")];
    char stringpool_str157[sizeof("MAC")];
    char stringpool_str158[sizeof("ISO-8859-14")];
    char stringpool_str159[sizeof("ISO_8859-14")];
    char stringpool_str160[sizeof("ISO8859-8")];
    char stringpool_str161[sizeof("ISO-IR-101")];
    char stringpool_str162[sizeof("ISO-8859-15")];
    char stringpool_str163[sizeof("ISO_8859-15")];
    char stringpool_str164[sizeof("ISO8859-9")];
    char stringpool_str166[sizeof("ISO-8859-8")];
    char stringpool_str167[sizeof("ISO_8859-8")];
    char stringpool_str170[sizeof("ISO-8859-9")];
    char stringpool_str171[sizeof("ISO_8859-9")];
    char stringpool_str172[sizeof("ISO_8859-14:1998")];
    char stringpool_str174[sizeof("ISO_8859-15:1998")];
    char stringpool_str176[sizeof("RK1048")];
    char stringpool_str179[sizeof("ELOT_928")];
    char stringpool_str180[sizeof("ISO8859-2")];
    char stringpool_str182[sizeof("IBM866")];
    char stringpool_str183[sizeof("L7")];
    char stringpool_str184[sizeof("US")];
    char stringpool_str186[sizeof("ISO-8859-2")];
    char stringpool_str187[sizeof("ISO_8859-2")];
    char stringpool_str190[sizeof("X0212")];
    char stringpool_str193[sizeof("CHAR")];
    char stringpool_str196[sizeof("PTCP154")];
    char stringpool_str197[sizeof("ISO-IR-109")];
    char stringpool_str198[sizeof("ISO-IR-138")];
    char stringpool_str199[sizeof("UHC")];
    char stringpool_str202[sizeof("ASCII")];
    char stringpool_str204[sizeof("MS-CYRL")];
    char stringpool_str205[sizeof("L10")];
    char stringpool_str206[sizeof("VISCII")];
    char stringpool_str209[sizeof("CP932")];
    char stringpool_str210[sizeof("BIG5")];
    char stringpool_str211[sizeof("TCVN")];
    char stringpool_str212[sizeof("CP50221")];
    char stringpool_str216[sizeof("BIG-5")];
    char stringpool_str217[sizeof("MS-EE")];
    char stringpool_str220[sizeof("850")];
    char stringpool_str222[sizeof("UCS-4")];
    char stringpool_str223[sizeof("CSASCII")];
    char stringpool_str224[sizeof("IBM819")];
    char stringpool_str225[sizeof("CSVISCII")];
    char stringpool_str227[sizeof("GB2312")];
    char stringpool_str228[sizeof("X0201")];
    char stringpool_str229[sizeof("CSBIG5")];
    char stringpool_str235[sizeof("CN-BIG5")];
    char stringpool_str236[sizeof("KOI8-R")];
    char stringpool_str237[sizeof("EUCKR")];
    char stringpool_str238[sizeof("IBM862")];
    char stringpool_str239[sizeof("CSPTCP154")];
    char stringpool_str240[sizeof("CSUCS4")];
    char stringpool_str241[sizeof("ISO646-US")];
    char stringpool_str242[sizeof("CP874")];
    char stringpool_str243[sizeof("EUC-KR")];
    char stringpool_str244[sizeof("VISCII1.1-1")];
    char stringpool_str246[sizeof("ISO_646.IRV:1991")];
    char stringpool_str248[sizeof("MACCYRILLIC")];
    char stringpool_str249[sizeof("CSKOI8R")];
    char stringpool_str250[sizeof("CP850")];
    char stringpool_str251[sizeof("ISO-IR-110")];
    char stringpool_str252[sizeof("CP950")];
    char stringpool_str254[sizeof("ISO-2022-CN")];
    char stringpool_str255[sizeof("CN-GB-ISOIR165")];
    char stringpool_str257[sizeof("CYRILLIC-ASIAN")];
    char stringpool_str258[sizeof("UCS-2")];
    char stringpool_str259[sizeof("LATIN10")];
    char stringpool_str260[sizeof("X0208")];
    char stringpool_str261[sizeof("LATIN3")];
    char stringpool_str263[sizeof("CSISO2022CN")];
    char stringpool_str264[sizeof("ISO_8859-10:1992")];
    char stringpool_str267[sizeof("UCS-4LE")];
    char stringpool_str268[sizeof("CP1250")];
    char stringpool_str271[sizeof("CSISOLATIN6")];
    char stringpool_str273[sizeof("ISO-IR-179")];
    char stringpool_str274[sizeof("MS936")];
    char stringpool_str275[sizeof("CSISOLATIN1")];
    char stringpool_str276[sizeof("TIS620")];
    char stringpool_str278[sizeof("CP1253")];
    char stringpool_str281[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str282[sizeof("TIS-620")];
    char stringpool_str285[sizeof("UCS-2LE")];
    char stringpool_str286[sizeof("KOI8-T")];
    char stringpool_str290[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str291[sizeof("CSISOLATIN4")];
    char stringpool_str295[sizeof("CSISOLATIN5")];
    char stringpool_str296[sizeof("UNICODE-1-1")];
    char stringpool_str301[sizeof("GREEK8")];
    char stringpool_str302[sizeof("UCS-4-INTERNAL")];
    char stringpool_str304[sizeof("CSUNICODE11")];
    char stringpool_str306[sizeof("CSUNICODE")];
    char stringpool_str307[sizeof("TIS620.2529-1")];
    char stringpool_str311[sizeof("NEXTSTEP")];
    char stringpool_str312[sizeof("GEORGIAN-PS")];
    char stringpool_str316[sizeof("ISO8859-10")];
    char stringpool_str318[sizeof("ISO8859-3")];
    char stringpool_str320[sizeof("UCS-2-INTERNAL")];
    char stringpool_str322[sizeof("ISO-8859-10")];
    char stringpool_str323[sizeof("ISO_8859-10")];
    char stringpool_str324[sizeof("ISO-8859-3")];
    char stringpool_str325[sizeof("ISO_8859-3")];
    char stringpool_str326[sizeof("ISO8859-13")];
    char stringpool_str327[sizeof("CSISOLATIN2")];
    char stringpool_str329[sizeof("GB_1988-80")];
    char stringpool_str332[sizeof("ISO-8859-13")];
    char stringpool_str333[sizeof("ISO_8859-13")];
    char stringpool_str337[sizeof("CP1133")];
    char stringpool_str339[sizeof("MS-ANSI")];
    char stringpool_str341[sizeof("ISO-IR-100")];
    char stringpool_str344[sizeof("KSC_5601")];
    char stringpool_str349[sizeof("MACINTOSH")];
    char stringpool_str352[sizeof("CN-GB")];
    char stringpool_str354[sizeof("CSIBM866")];
    char stringpool_str355[sizeof("ISO-10646-UCS-4")];
    char stringpool_str357[sizeof("ISO_8859-4:1988")];
    char stringpool_str359[sizeof("ISO_8859-5:1988")];
    char stringpool_str361[sizeof("UTF-16")];
    char stringpool_str363[sizeof("KZ-1048")];
    char stringpool_str364[sizeof("ROMAN8")];
    char stringpool_str365[sizeof("ISO_8859-8:1988")];
    char stringpool_str366[sizeof("UNICODELITTLE")];
    char stringpool_str369[sizeof("ISO_8859-9:1989")];
    char stringpool_str372[sizeof("JP")];
    char stringpool_str373[sizeof("ISO-10646-UCS-2")];
    char stringpool_str375[sizeof("MULELAO-1")];
    char stringpool_str376[sizeof("CSKZ1048")];
    char stringpool_str377[sizeof("ISO-IR-203")];
    char stringpool_str378[sizeof("IBM850")];
    char stringpool_str380[sizeof("TIS620-0")];
    char stringpool_str382[sizeof("ECMA-114")];
    char stringpool_str386[sizeof("KS_C_5601-1989")];
    char stringpool_str388[sizeof("GBK")];
    char stringpool_str389[sizeof("UTF-8")];
    char stringpool_str391[sizeof("ARMSCII-8")];
    char stringpool_str392[sizeof("US-ASCII")];
    char stringpool_str398[sizeof("ECMA-118")];
    char stringpool_str399[sizeof("CSGB2312")];
    char stringpool_str403[sizeof("LATIN7")];
    char stringpool_str404[sizeof("KOREAN")];
    char stringpool_str405[sizeof("ISO-IR-57")];
    char stringpool_str408[sizeof("MS-HEBR")];
    char stringpool_str409[sizeof("CSISO14JISC6220RO")];
    char stringpool_str411[sizeof("ISO-IR-87")];
    char stringpool_str413[sizeof("ISO-IR-157")];
    char stringpool_str416[sizeof("UTF-16LE")];
    char stringpool_str418[sizeof("UCS-4BE")];
    char stringpool_str420[sizeof("CP1257")];
    char stringpool_str421[sizeof("ANSI_X3.4-1986")];
    char stringpool_str424[sizeof("CSEUCKR")];
    char stringpool_str425[sizeof("ISO-2022-KR")];
    char stringpool_str426[sizeof("GB_2312-80")];
    char stringpool_str429[sizeof("ISO-IR-127")];
    char stringpool_str430[sizeof("MACTHAI")];
    char stringpool_str431[sizeof("EUCTW")];
    char stringpool_str433[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str434[sizeof("CSISO2022KR")];
    char stringpool_str436[sizeof("UCS-2BE")];
    char stringpool_str437[sizeof("EUC-TW")];
    char stringpool_str439[sizeof("ANSI_X3.4-1968")];
    char stringpool_str440[sizeof("GREEK")];
    char stringpool_str442[sizeof("MACICELAND")];
    char stringpool_str443[sizeof("GB18030")];
    char stringpool_str444[sizeof("ISO_8859-3:1988")];
    char stringpool_str445[sizeof("CSISOLATINARABIC")];
    char stringpool_str446[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str448[sizeof("HP-ROMAN8")];
    char stringpool_str453[sizeof("TIS620.2533-1")];
    char stringpool_str455[sizeof("UNICODEBIG")];
    char stringpool_str459[sizeof("TCVN5712-1")];
    char stringpool_str460[sizeof("ISO8859-7")];
    char stringpool_str461[sizeof("CSISO57GB1988")];
    char stringpool_str463[sizeof("MACCENTRALEUROPE")];
    char stringpool_str464[sizeof("CSISOLATINGREEK")];
    char stringpool_str465[sizeof("CSISOLATIN3")];
    char stringpool_str466[sizeof("ISO-8859-7")];
    char stringpool_str467[sizeof("ISO_8859-7")];
    char stringpool_str468[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str469[sizeof("CP367")];
    char stringpool_str470[sizeof("HZ-GB-2312")];
    char stringpool_str473[sizeof("EUCJP")];
    char stringpool_str474[sizeof("CSUNICODE11UTF7")];
    char stringpool_str476[sizeof("MACROMAN")];
    char stringpool_str477[sizeof("TCVN-5712")];
    char stringpool_str479[sizeof("EUC-JP")];
    char stringpool_str483[sizeof("ISO646-JP")];
    char stringpool_str489[sizeof("BIG5HKSCS")];
    char stringpool_str490[sizeof("CSISO58GB231280")];
    char stringpool_str492[sizeof("WINDOWS-1256")];
    char stringpool_str494[sizeof("WINDOWS-1251")];
    char stringpool_str495[sizeof("BIG5-HKSCS")];
    char stringpool_str496[sizeof("WCHAR_T")];
    char stringpool_str497[sizeof("ISO_8859-6:1987")];
    char stringpool_str499[sizeof("ISO_8859-1:1987")];
    char stringpool_str502[sizeof("WINDOWS-1254")];
    char stringpool_str504[sizeof("WINDOWS-1255")];
    char stringpool_str510[sizeof("WINDOWS-1258")];
    char stringpool_str512[sizeof("UTF-32")];
    char stringpool_str516[sizeof("CSISOLATINHEBREW")];
    char stringpool_str517[sizeof("CSHPROMAN8")];
    char stringpool_str519[sizeof("CSISO159JISX02121990")];
    char stringpool_str520[sizeof("WINDOWS-1252")];
    char stringpool_str522[sizeof("STRK1048-2002")];
    char stringpool_str525[sizeof("ISO_8859-2:1987")];
    char stringpool_str529[sizeof("ARABIC")];
    char stringpool_str530[sizeof("CSMACINTOSH")];
    char stringpool_str534[sizeof("KS_C_5601-1987")];
    char stringpool_str536[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str537[sizeof("JIS_C6226-1983")];
    char stringpool_str539[sizeof("UTF-32LE")];
    char stringpool_str543[sizeof("TIS620.2533-0")];
    char stringpool_str546[sizeof("IBM-CP1133")];
    char stringpool_str547[sizeof("SHIFT-JIS")];
    char stringpool_str548[sizeof("SHIFT_JIS")];
    char stringpool_str555[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str557[sizeof("BIGFIVE")];
    char stringpool_str560[sizeof("CSKSC56011987")];
    char stringpool_str562[sizeof("MACCROATIAN")];
    char stringpool_str563[sizeof("BIG-FIVE")];
    char stringpool_str566[sizeof("WINDOWS-936")];
    char stringpool_str567[sizeof("UTF-16BE")];
    char stringpool_str574[sizeof("KOI8-U")];
    char stringpool_str580[sizeof("KOI8-RU")];
    char stringpool_str584[sizeof("WINDOWS-1250")];
    char stringpool_str585[sizeof("MS-GREEK")];
    char stringpool_str589[sizeof("WINDOWS-1253")];
    char stringpool_str591[sizeof("MACGREEK")];
    char stringpool_str592[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str594[sizeof("ISO_8859-7:2003")];
    char stringpool_str596[sizeof("JIS0208")];
    char stringpool_str597[sizeof("IBM367")];
    char stringpool_str608[sizeof("ISO-2022-JP-1")];
    char stringpool_str610[sizeof("ISO-2022-JP-MS")];
    char stringpool_str613[sizeof("JIS_X0212")];
    char stringpool_str618[sizeof("CSEUCTW")];
    char stringpool_str624[sizeof("CSSHIFTJIS")];
    char stringpool_str625[sizeof("HEBREW")];
    char stringpool_str634[sizeof("ISO-2022-JP-2")];
    char stringpool_str635[sizeof("ASMO-708")];
    char stringpool_str642[sizeof("CSISO2022JP2")];
    char stringpool_str651[sizeof("JIS_X0201")];
    char stringpool_str655[sizeof("WINDOWS-874")];
    char stringpool_str660[sizeof("WINDOWS-1257")];
    char stringpool_str661[sizeof("ISO-2022-JP")];
    char stringpool_str665[sizeof("ISO_8859-7:1987")];
    char stringpool_str666[sizeof("CSISO87JISX0208")];
    char stringpool_str670[sizeof("CSISO2022JP")];
    char stringpool_str680[sizeof("MACARABIC")];
    char stringpool_str683[sizeof("JIS_X0208")];
    char stringpool_str684[sizeof("JISX0201-1976")];
    char stringpool_str689[sizeof("UTF-7")];
    char stringpool_str690[sizeof("UTF-32BE")];
    char stringpool_str692[sizeof("BIG5-HKSCS:2001")];
    char stringpool_str694[sizeof("JIS_X0212-1990")];
    char stringpool_str697[sizeof("JIS_X0212.1990-0")];
    char stringpool_str700[sizeof("BIG5-HKSCS:2004")];
    char stringpool_str702[sizeof("MACTURKISH")];
    char stringpool_str708[sizeof("BIG5-HKSCS:2008")];
    char stringpool_str710[sizeof("BIG5-HKSCS:1999")];
    char stringpool_str725[sizeof("MS-TURK")];
    char stringpool_str732[sizeof("UCS-4-SWAPPED")];
    char stringpool_str740[sizeof("GB18030:2022")];
    char stringpool_str749[sizeof("MACUKRAINE")];
    char stringpool_str750[sizeof("UCS-2-SWAPPED")];
    char stringpool_str752[sizeof("TCVN5712-1:1993")];
    char stringpool_str774[sizeof("JIS_X0208-1990")];
    char stringpool_str779[sizeof("JIS_X0208-1983")];
    char stringpool_str788[sizeof("GB18030:2005")];
    char stringpool_str796[sizeof("MACHEBREW")];
    char stringpool_str803[sizeof("MS-ARAB")];
    char stringpool_str811[sizeof("JAVA")];
    char stringpool_str813[sizeof("MACROMANIA")];
    char stringpool_str827[sizeof("MS_KANJI")];
    char stringpool_str837[sizeof("WINBALTRIM")];
    char stringpool_str866[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str869[sizeof("JOHAB")];
    char stringpool_str921[sizeof("CSHALFWIDTHKATAKANA")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "L6",
    "L1",
    "CN",
    "L4",
    "L5",
    "R8",
    "L8",
    "SJIS",
    "866",
    "L2",
    "ISO-IR-6",
    "CP866",
    "C99",
    "ISO-IR-166",
    "EUCCN",
    "LATIN6",
    "CP154",
    "LATIN1",
    "EUC-CN",
    "CYRILLIC",
    "ISO646-CN",
    "HZ",
    "ISO-IR-14",
    "CP1256",
    "LATIN4",
    "CP1251",
    "ISO-IR-165",
    "LATIN5",
    "862",
    "ISO-IR-126",
    "ISO-IR-144",
    "CP819",
    "LATIN8",
    "CP1254",
    "ISO-IR-58",
    "CP949",
    "CP1255",
    "CP862",
    "ISO-IR-148",
    "L3",
    "LATIN-9",
    "CHINESE",
    "ISO-IR-149",
    "ISO-IR-159",
    "ISO-IR-226",
    "CP1258",
    "PT154",
    "LATIN2",
    "ISO8859-6",
    "ISO-IR-199",
    "ISO8859-1",
    "ISO-CELTIC",
    "ISO-8859-6",
    "ISO_8859-6",
    "ISO8859-16",
    "ISO-8859-1",
    "ISO_8859-1",
    "ISO8859-11",
    "ISO-8859-16",
    "ISO_8859-16",
    "CP1252",
    "ISO-8859-11",
    "ISO_8859-11",
    "ISO8859-4",
    "CP1361",
    "ISO_8859-16:2001",
    "CP1131",
    "ISO8859-5",
    "ISO-8859-4",
    "ISO_8859-4",
    "ISO8859-14",
    "CP936",
    "ISO-8859-5",
    "ISO_8859-5",
    "ISO8859-15",
    "MAC",
    "ISO-8859-14",
    "ISO_8859-14",
    "ISO8859-8",
    "ISO-IR-101",
    "ISO-8859-15",
    "ISO_8859-15",
    "ISO8859-9",
    "ISO-8859-8",
    "ISO_8859-8",
    "ISO-8859-9",
    "ISO_8859-9",
    "ISO_8859-14:1998",
    "ISO_8859-15:1998",
    "RK1048",
    "ELOT_928",
    "ISO8859-2",
    "IBM866",
    "L7",
    "US",
    "ISO-8859-2",
    "ISO_8859-2",
    "X0212",
    "CHAR",
    "PTCP154",
    "ISO-IR-109",
    "ISO-IR-138",
    "UHC",
    "ASCII",
    "MS-CYRL",
    "L10",
    "VISCII",
    "CP932",
    "BIG5",
    "TCVN",
    "CP50221",
    "BIG-5",
    "MS-EE",
    "850",
    "UCS-4",
    "CSASCII",
    "IBM819",
    "CSVISCII",
    "GB2312",
    "X0201",
    "CSBIG5",
    "CN-BIG5",
    "KOI8-R",
    "EUCKR",
    "IBM862",
    "CSPTCP154",
    "CSUCS4",
    "ISO646-US",
    "CP874",
    "EUC-KR",
    "VISCII1.1-1",
    "ISO_646.IRV:1991",
    "MACCYRILLIC",
    "CSKOI8R",
    "CP850",
    "ISO-IR-110",
    "CP950",
    "ISO-2022-CN",
    "CN-GB-ISOIR165",
    "CYRILLIC-ASIAN",
    "UCS-2",
    "LATIN10",
    "X0208",
    "LATIN3",
    "CSISO2022CN",
    "ISO_8859-10:1992",
    "UCS-4LE",
    "CP1250",
    "CSISOLATIN6",
    "ISO-IR-179",
    "MS936",
    "CSISOLATIN1",
    "TIS620",
    "CP1253",
    "ISO-2022-CN-EXT",
    "TIS-620",
    "UCS-2LE",
    "KOI8-T",
    "CSISOLATINCYRILLIC",
    "CSISOLATIN4",
    "CSISOLATIN5",
    "UNICODE-1-1",
    "GREEK8",
    "UCS-4-INTERNAL",
    "CSUNICODE11",
    "CSUNICODE",
    "TIS620.2529-1",
    "NEXTSTEP",
    "GEORGIAN-PS",
    "ISO8859-10",
    "ISO8859-3",
    "UCS-2-INTERNAL",
    "ISO-8859-10",
    "ISO_8859-10",
    "ISO-8859-3",
    "ISO_8859-3",
    "ISO8859-13",
    "CSISOLATIN2",
    "GB_1988-80",
    "ISO-8859-13",
    "ISO_8859-13",
    "CP1133",
    "MS-ANSI",
    "ISO-IR-100",
    "KSC_5601",
    "MACINTOSH",
    "CN-GB",
    "CSIBM866",
    "ISO-10646-UCS-4",
    "ISO_8859-4:1988",
    "ISO_8859-5:1988",
    "UTF-16",
    "KZ-1048",
    "ROMAN8",
    "ISO_8859-8:1988",
    "UNICODELITTLE",
    "ISO_8859-9:1989",
    "JP",
    "ISO-10646-UCS-2",
    "MULELAO-1",
    "CSKZ1048",
    "ISO-IR-203",
    "IBM850",
    "TIS620-0",
    "ECMA-114",
    "KS_C_5601-1989",
    "GBK",
    "UTF-8",
    "ARMSCII-8",
    "US-ASCII",
    "ECMA-118",
    "CSGB2312",
    "LATIN7",
    "KOREAN",
    "ISO-IR-57",
    "MS-HEBR",
    "CSISO14JISC6220RO",
    "ISO-IR-87",
    "ISO-IR-157",
    "UTF-16LE",
    "UCS-4BE",
    "CP1257",
    "ANSI_X3.4-1986",
    "CSEUCKR",
    "ISO-2022-KR",
    "GB_2312-80",
    "ISO-IR-127",
    "MACTHAI",
    "EUCTW",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "CSISO2022KR",
    "UCS-2BE",
    "EUC-TW",
    "ANSI_X3.4-1968",
    "GREEK",
    "MACICELAND",
    "GB18030",
    "ISO_8859-3:1988",
    "CSISOLATINARABIC",
    "GEORGIAN-ACADEMY",
    "HP-ROMAN8",
    "TIS620.2533-1",
    "UNICODEBIG",
    "TCVN5712-1",
    "ISO8859-7",
    "CSISO57GB1988",
    "MACCENTRALEUROPE",
    "CSISOLATINGREEK",
    "CSISOLATIN3",
    "ISO-8859-7",
    "ISO_8859-7",
    "UNICODE-1-1-UTF-7",
    "CP367",
    "HZ-GB-2312",
    "EUCJP",
    "CSUNICODE11UTF7",
    "MACROMAN",
    "TCVN-5712",
    "EUC-JP",
    "ISO646-JP",
    "BIG5HKSCS",
    "CSISO58GB231280",
    "WINDOWS-1256",
    "WINDOWS-1251",
    "BIG5-HKSCS",
    "WCHAR_T",
    "ISO_8859-6:1987",
    "ISO_8859-1:1987",
    "WINDOWS-1254",
    "WINDOWS-1255",
    "WINDOWS-1258",
    "UTF-32",
    "CSISOLATINHEBREW",
    "CSHPROMAN8",
    "CSISO159JISX02121990",
    "WINDOWS-1252",
    "STRK1048-2002",
    "ISO_8859-2:1987",
    "ARABIC",
    "CSMACINTOSH",
    "KS_C_5601-1987",
    "JIS_C6220-1969-RO",
    "JIS_C6226-1983",
    "UTF-32LE",
    "TIS620.2533-0",
    "IBM-CP1133",
    "SHIFT-JIS",
    "SHIFT_JIS",
    "CSPC862LATINHEBREW",
    "BIGFIVE",
    "CSKSC56011987",
    "MACCROATIAN",
    "BIG-FIVE",
    "WINDOWS-936",
    "UTF-16BE",
    "KOI8-U",
    "KOI8-RU",
    "WINDOWS-1250",
    "MS-GREEK",
    "WINDOWS-1253",
    "MACGREEK",
    "CSPC850MULTILINGUAL",
    "ISO_8859-7:2003",
    "JIS0208",
    "IBM367",
    "ISO-2022-JP-1",
    "ISO-2022-JP-MS",
    "JIS_X0212",
    "CSEUCTW",
    "CSSHIFTJIS",
    "HEBREW",
    "ISO-2022-JP-2",
    "ASMO-708",
    "CSISO2022JP2",
    "JIS_X0201",
    "WINDOWS-874",
    "WINDOWS-1257",
    "ISO-2022-JP",
    "ISO_8859-7:1987",
    "CSISO87JISX0208",
    "CSISO2022JP",
    "MACARABIC",
    "JIS_X0208",
    "JISX0201-1976",
    "UTF-7",
    "UTF-32BE",
    "BIG5-HKSCS:2001",
    "JIS_X0212-1990",
    "JIS_X0212.1990-0",
    "BIG5-HKSCS:2004",
    "MACTURKISH",
    "BIG5-HKSCS:2008",
    "BIG5-HKSCS:1999",
    "MS-TURK",
    "UCS-4-SWAPPED",
    "GB18030:2022",
    "MACUKRAINE",
    "UCS-2-SWAPPED",
    "TCVN5712-1:1993",
    "JIS_X0208-1990",
    "JIS_X0208-1983",
    "GB18030:2005",
    "MACHEBREW",
    "MS-ARAB",
    "JAVA",
    "MACROMANIA",
    "MS_KANJI",
    "WINBALTRIM",
    "CSEUCPKDFMTJAPANESE",
    "JOHAB",
    "CSHALFWIDTHKATAKANA"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 134 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15, ei_iso8859_10},
    {-1},
#line 60 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17, ei_iso8859_1},
    {-1}, {-1},
#line 288 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str20, ei_iso646_cn},
    {-1}, {-1}, {-1}, {-1},
#line 84 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25, ei_iso8859_4},
    {-1},
#line 126 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str27, ei_iso8859_9},
    {-1}, {-1},
#line 227 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30, ei_hp_roman8},
    {-1}, {-1},
#line 151 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str33, ei_iso8859_14},
#line 308 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34, ei_sjis},
    {-1},
#line 207 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36, ei_cp866},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 68 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43, ei_iso8859_2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 16 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str51, ei_ascii},
    {-1}, {-1},
#line 205 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54, ei_cp866},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 51 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64, ei_c99},
#line 252 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65, ei_tis620},
#line 320 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str66, ei_euc_cn},
#line 133 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67, ei_iso8859_10},
    {-1}, {-1},
#line 236 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str70, ei_pt154},
#line 59 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71, ei_iso8859_1},
#line 319 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str72, ei_euc_cn},
    {-1},
#line 91 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str74, ei_iso8859_5},
    {-1},
#line 286 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str76, ei_iso646_cn},
    {-1},
#line 334 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str78, ei_hz},
#line 264 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79, ei_iso646_jp},
    {-1}, {-1}, {-1}, {-1},
#line 189 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str84, ei_cp1256},
    {-1}, {-1},
#line 83 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str87, ei_iso8859_4},
#line 174 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str88, ei_cp1251},
#line 294 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str89, ei_isoir165},
    {-1},
#line 125 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str91, ei_iso8859_9},
#line 203 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92, ei_cp862},
#line 107 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str93, ei_iso8859_7},
    {-1},
#line 90 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95, ei_iso8859_5},
#line 57 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96, ei_iso8859_1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 150 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103, ei_iso8859_14},
#line 183 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str104, ei_cp1254},
#line 291 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105, ei_gb2312},
#line 355 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106, ei_cp949},
    {-1},
#line 186 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108, ei_cp1255},
    {-1},
#line 201 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110, ei_cp862},
#line 124 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111, ei_iso8859_9},
#line 76 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112, ei_iso8859_3},
#line 158 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str113, ei_iso8859_15},
#line 293 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str114, ei_gb2312},
#line 299 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115, ei_ksc5601},
    {-1},
#line 283 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117, ei_jisx0212},
    {-1},
#line 163 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119, ei_iso8859_16},
#line 195 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str120, ei_cp1258},
#line 234 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121, ei_pt154},
    {-1},
#line 67 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str123, ei_iso8859_2},
#line 102 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124, ei_iso8859_6},
#line 149 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125, ei_iso8859_14},
    {-1}, {-1},
#line 62 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128, ei_iso8859_1},
#line 152 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129, ei_iso8859_14},
#line 94 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130, ei_iso8859_6},
#line 95 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131, ei_iso8859_6},
#line 166 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str132, ei_iso8859_16},
    {-1},
#line 53 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str134, ei_iso8859_1},
#line 54 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str135, ei_iso8859_1},
#line 139 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136, ei_iso8859_11},
    {-1},
#line 160 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138, ei_iso8859_16},
#line 161 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139, ei_iso8859_16},
#line 177 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140, ei_cp1252},
    {-1},
#line 137 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142, ei_iso8859_11},
#line 138 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143, ei_iso8859_11},
#line 86 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str144, ei_iso8859_4},
#line 358 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145, ei_johab},
#line 162 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146, ei_iso8859_16},
#line 209 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147, ei_cp1131},
#line 93 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148, ei_iso8859_5},
    {-1},
#line 79 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150, ei_iso8859_4},
#line 80 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151, ei_iso8859_4},
#line 153 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152, ei_iso8859_14},
#line 325 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153, ei_cp936},
#line 87 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154, ei_iso8859_5},
#line 88 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155, ei_iso8859_5},
#line 159 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156, ei_iso8859_15},
#line 212 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157, ei_mac_roman},
#line 146 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158, ei_iso8859_14},
#line 147 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159, ei_iso8859_14},
#line 120 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160, ei_iso8859_8},
#line 66 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str161, ei_iso8859_2},
#line 154 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str162, ei_iso8859_15},
#line 155 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163, ei_iso8859_15},
#line 128 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str164, ei_iso8859_9},
    {-1},
#line 114 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166, ei_iso8859_8},
#line 115 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167, ei_iso8859_8},
    {-1}, {-1},
#line 121 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170, ei_iso8859_9},
#line 122 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171, ei_iso8859_9},
#line 148 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172, ei_iso8859_14},
    {-1},
#line 156 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174, ei_iso8859_15},
    {-1},
#line 239 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176, ei_rk1048},
    {-1}, {-1},
#line 109 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_7},
#line 70 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180, ei_iso8859_2},
    {-1},
#line 206 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182, ei_cp866},
#line 144 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183, ei_iso8859_13},
#line 21 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184, ei_ascii},
    {-1},
#line 63 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str186, ei_iso8859_2},
#line 64 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187, ei_iso8859_2},
    {-1}, {-1},
#line 282 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190, ei_jisx0212},
    {-1}, {-1},
#line 361 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193, ei_local_char},
    {-1}, {-1},
#line 235 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196, ei_pt154},
#line 74 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197, ei_iso8859_3},
#line 117 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198, ei_iso8859_8},
#line 356 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199, ei_cp949},
    {-1}, {-1},
#line 13 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202, ei_ascii},
    {-1},
#line 176 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str204, ei_cp1251},
#line 165 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205, ei_iso8859_16},
#line 255 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str206, ei_viscii},
    {-1}, {-1},
#line 311 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209, ei_cp932},
#line 339 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str210, ei_ces_big5},
#line 258 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211, ei_tcvn},
#line 318 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212, ei_iso2022_jpms},
    {-1}, {-1}, {-1},
#line 340 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216, ei_ces_big5},
#line 173 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str217, ei_cp1250},
    {-1}, {-1},
#line 199 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220, ei_cp850},
    {-1},
#line 33 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str222, ei_ucs4},
#line 22 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223, ei_ascii},
#line 58 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str224, ei_iso8859_1},
#line 257 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225, ei_viscii},
    {-1},
#line 321 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227, ei_euc_cn},
#line 269 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228, ei_jisx0201},
#line 344 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 343 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235, ei_ces_big5},
#line 167 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236, ei_koi8_r},
#line 353 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237, ei_euc_kr},
#line 202 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238, ei_cp862},
#line 238 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239, ei_pt154},
#line 35 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240, ei_ucs4},
#line 14 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241, ei_ascii},
#line 253 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242, ei_cp874},
#line 352 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243, ei_euc_kr},
#line 256 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str244, ei_viscii},
    {-1},
#line 15 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246, ei_ascii},
    {-1},
#line 218 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248, ei_mac_cyrillic},
#line 168 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249, ei_koi8_r},
#line 197 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str250, ei_cp850},
#line 82 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str251, ei_iso8859_4},
#line 345 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str252, ei_cp950},
    {-1},
#line 331 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str254, ei_iso2022_cn},
#line 295 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str255, ei_isoir165},
    {-1},
#line 237 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str257, ei_pt154},
#line 24 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258, ei_ucs2},
#line 164 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259, ei_iso8859_16},
#line 275 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260, ei_jisx0208},
#line 75 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261, ei_iso8859_3},
    {-1},
#line 332 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str263, ei_iso2022_cn},
#line 131 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264, ei_iso8859_10},
    {-1}, {-1},
#line 37 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267, ei_ucs4le},
#line 171 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268, ei_cp1250},
    {-1}, {-1},
#line 135 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271, ei_iso8859_10},
    {-1},
#line 142 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str273, ei_iso8859_13},
#line 326 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274, ei_cp936},
#line 61 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275, ei_iso8859_1},
#line 247 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276, ei_tis620},
    {-1},
#line 180 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278, ei_cp1253},
    {-1}, {-1},
#line 333 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281, ei_iso2022_cn_ext},
#line 246 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282, ei_tis620},
    {-1}, {-1},
#line 31 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285, ei_ucs2le},
#line 233 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str286, ei_koi8_t},
    {-1}, {-1}, {-1},
#line 92 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290, ei_iso8859_5},
#line 85 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291, ei_iso8859_4},
    {-1}, {-1}, {-1},
#line 127 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295, ei_iso8859_9},
#line 29 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296, ei_ucs2be},
    {-1}, {-1}, {-1}, {-1},
#line 110 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301, ei_iso8859_7},
#line 49 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302, ei_ucs4internal},
    {-1},
#line 30 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str304, ei_ucs2be},
    {-1},
#line 26 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306, ei_ucs2},
#line 249 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307, ei_tis620},
    {-1}, {-1}, {-1},
#line 229 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str311, ei_nextstep},
#line 232 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312, ei_georgian_ps},
    {-1}, {-1}, {-1},
#line 136 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str316, ei_iso8859_10},
    {-1},
#line 78 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318, ei_iso8859_3},
    {-1},
#line 47 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str320, ei_ucs2internal},
    {-1},
#line 129 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322, ei_iso8859_10},
#line 130 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str323, ei_iso8859_10},
#line 71 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str324, ei_iso8859_3},
#line 72 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str325, ei_iso8859_3},
#line 145 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str326, ei_iso8859_13},
#line 69 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327, ei_iso8859_2},
    {-1},
#line 285 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str329, ei_iso646_cn},
    {-1}, {-1},
#line 140 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332, ei_iso8859_13},
#line 141 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str333, ei_iso8859_13},
    {-1}, {-1}, {-1},
#line 244 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337, ei_cp1133},
    {-1},
#line 179 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str339, ei_cp1252},
    {-1},
#line 56 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str341, ei_iso8859_1},
    {-1}, {-1},
#line 296 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str344, ei_ksc5601},
    {-1}, {-1}, {-1}, {-1},
#line 211 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349, ei_mac_roman},
    {-1}, {-1},
#line 322 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352, ei_euc_cn},
    {-1},
#line 208 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354, ei_cp866},
#line 34 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355, ei_ucs4},
    {-1},
#line 81 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str357, ei_iso8859_4},
    {-1},
#line 89 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str359, ei_iso8859_5},
    {-1},
#line 38 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str361, ei_utf16},
    {-1},
#line 241 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str363, ei_rk1048},
#line 226 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364, ei_hp_roman8},
#line 116 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365, ei_iso8859_8},
#line 32 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str366, ei_ucs2le},
    {-1}, {-1},
#line 123 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str369, ei_iso8859_9},
    {-1}, {-1},
#line 265 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372, ei_iso646_jp},
#line 25 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373, ei_ucs2},
    {-1},
#line 243 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375, ei_mulelao},
#line 242 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str376, ei_rk1048},
#line 157 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str377, ei_iso8859_15},
#line 198 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str378, ei_cp850},
    {-1},
#line 248 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str380, ei_tis620},
    {-1},
#line 98 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str382, ei_iso8859_6},
    {-1}, {-1}, {-1},
#line 298 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386, ei_ksc5601},
    {-1},
#line 324 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str388, ei_ces_gbk},
#line 23 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str389, ei_utf8},
    {-1},
#line 230 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391, ei_armscii_8},
#line 12 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392, ei_ascii},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 108 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str398, ei_iso8859_7},
#line 323 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str399, ei_euc_cn},
    {-1}, {-1}, {-1},
#line 143 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str403, ei_iso8859_13},
#line 301 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404, ei_ksc5601},
#line 287 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405, ei_iso646_cn},
    {-1}, {-1},
#line 188 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str408, ei_cp1255},
#line 266 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str409, ei_iso646_jp},
    {-1},
#line 276 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str411, ei_jisx0208},
    {-1},
#line 132 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str413, ei_iso8859_10},
    {-1}, {-1},
#line 40 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416, ei_utf16le},
    {-1},
#line 36 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418, ei_ucs4be},
    {-1},
#line 192 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str420, ei_cp1257},
#line 18 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421, ei_ascii},
    {-1}, {-1},
#line 354 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424, ei_euc_kr},
#line 359 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425, ei_iso2022_kr},
#line 290 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426, ei_gb2312},
    {-1}, {-1},
#line 97 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429, ei_iso8859_6},
#line 224 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430, ei_mac_thai},
#line 337 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431, ei_euc_tw},
    {-1},
#line 304 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433, ei_euc_jp},
#line 360 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str434, ei_iso2022_kr},
    {-1},
#line 27 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str436, ei_ucs2be},
#line 336 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437, ei_euc_tw},
    {-1},
#line 17 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str439, ei_ascii},
#line 111 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str440, ei_iso8859_7},
    {-1},
#line 215 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str442, ei_mac_iceland},
#line 328 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str443, ei_gb18030_2005},
#line 73 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str444, ei_iso8859_3},
#line 101 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445, ei_iso8859_6},
#line 231 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446, ei_georgian_academy},
    {-1},
#line 225 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1},
#line 251 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str453, ei_tis620},
    {-1},
#line 28 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str455, ei_ucs2be},
    {-1}, {-1}, {-1},
#line 260 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459, ei_tcvn},
#line 113 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460, ei_iso8859_7},
#line 289 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str461, ei_iso646_cn},
    {-1},
#line 214 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str463, ei_mac_centraleurope},
#line 112 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str464, ei_iso8859_7},
#line 77 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str465, ei_iso8859_3},
#line 103 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str466, ei_iso8859_7},
#line 104 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467, ei_iso8859_7},
#line 45 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468, ei_utf7},
#line 19 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469, ei_ascii},
#line 335 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470, ei_hz},
    {-1}, {-1},
#line 303 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str473, ei_euc_jp},
#line 46 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474, ei_utf7},
    {-1},
#line 210 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str476, ei_mac_roman},
#line 259 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477, ei_tcvn},
    {-1},
#line 302 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str479, ei_euc_jp},
    {-1}, {-1}, {-1},
#line 263 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str483, ei_iso646_jp},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 350 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str489, ei_big5hkscs2008},
#line 292 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490, ei_gb2312},
    {-1},
#line 190 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str492, ei_cp1256},
    {-1},
#line 175 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str494, ei_cp1251},
#line 349 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495, ei_big5hkscs2008},
#line 362 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str496, ei_local_wchar_t},
#line 96 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str497, ei_iso8859_6},
    {-1},
#line 55 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str499, ei_iso8859_1},
    {-1}, {-1},
#line 184 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str502, ei_cp1254},
    {-1},
#line 187 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str504, ei_cp1255},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 196 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str510, ei_cp1258},
    {-1},
#line 41 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str512, ei_utf32},
    {-1}, {-1}, {-1},
#line 119 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str516, ei_iso8859_8},
#line 228 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str517, ei_hp_roman8},
    {-1},
#line 284 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str519, ei_jisx0212},
#line 178 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str520, ei_cp1252},
    {-1},
#line 240 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str522, ei_rk1048},
    {-1}, {-1},
#line 65 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str525, ei_iso8859_2},
    {-1}, {-1}, {-1},
#line 100 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str529, ei_iso8859_6},
#line 213 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str530, ei_mac_roman},
    {-1}, {-1}, {-1},
#line 297 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str534, ei_ksc5601},
    {-1},
#line 262 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str536, ei_iso646_jp},
#line 277 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str537, ei_jisx0208},
    {-1},
#line 43 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str539, ei_utf32le},
    {-1}, {-1}, {-1},
#line 250 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str543, ei_tis620},
    {-1}, {-1},
#line 245 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str546, ei_cp1133},
#line 307 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str547, ei_sjis},
#line 306 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str548, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 204 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str555, ei_cp862},
    {-1},
#line 342 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str557, ei_ces_big5},
    {-1}, {-1},
#line 300 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str560, ei_ksc5601},
    {-1},
#line 216 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str562, ei_mac_croatian},
#line 341 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str563, ei_ces_big5},
    {-1}, {-1},
#line 327 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str566, ei_cp936},
#line 39 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str567, ei_utf16be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 169 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str574, ei_koi8_u},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 170 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str580, ei_koi8_ru},
    {-1}, {-1}, {-1},
#line 172 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str584, ei_cp1250},
#line 182 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str585, ei_cp1253},
    {-1}, {-1}, {-1},
#line 181 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str589, ei_cp1253},
    {-1},
#line 220 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str591, ei_mac_greek},
#line 200 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str592, ei_cp850},
    {-1},
#line 106 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str594, ei_iso8859_7},
    {-1},
#line 274 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str596, ei_jisx0208},
#line 20 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str597, ei_ascii},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 314 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str608, ei_iso2022_jp1},
    {-1},
#line 317 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str610, ei_iso2022_jpms},
    {-1}, {-1},
#line 279 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str613, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1},
#line 338 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str618, ei_euc_tw},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 310 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str624, ei_sjis},
#line 118 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str625, ei_iso8859_8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 315 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str634, ei_iso2022_jp2},
#line 99 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str635, ei_iso8859_6},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 316 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str642, ei_iso2022_jp2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 267 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str651, ei_jisx0201},
    {-1}, {-1}, {-1},
#line 254 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str655, ei_cp874},
    {-1}, {-1}, {-1}, {-1},
#line 193 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str660, ei_cp1257},
#line 312 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str661, ei_iso2022_jp},
    {-1}, {-1}, {-1},
#line 105 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str665, ei_iso8859_7},
#line 278 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str666, ei_jisx0208},
    {-1}, {-1}, {-1},
#line 313 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str670, ei_iso2022_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 223 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str680, ei_mac_arabic},
    {-1}, {-1},
#line 271 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str683, ei_jisx0208},
#line 268 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str684, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1},
#line 44 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str689, ei_utf7},
#line 42 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str690, ei_utf32be},
    {-1},
#line 347 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str692, ei_big5hkscs2001},
    {-1},
#line 281 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str694, ei_jisx0212},
    {-1}, {-1},
#line 280 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str697, ei_jisx0212},
    {-1}, {-1},
#line 348 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str700, ei_big5hkscs2004},
    {-1},
#line 221 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str702, ei_mac_turkish},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 351 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str708, ei_big5hkscs2008},
    {-1},
#line 346 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str710, ei_big5hkscs1999},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 185 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str725, ei_cp1254},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 50 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str732, ei_ucs4swapped},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 330 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str740, ei_gb18030_2022},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 219 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str749, ei_mac_ukraine},
#line 48 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str750, ei_ucs2swapped},
    {-1},
#line 261 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str752, ei_tcvn},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 273 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str774, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1},
#line 272 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str779, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 329 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str788, ei_gb18030_2005},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 222 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str796, ei_mac_hebrew},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 191 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str803, ei_cp1256},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 52 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str811, ei_java},
    {-1},
#line 217 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str813, ei_mac_romania},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 309 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str827, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 194 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str837, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 305 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str866, ei_euc_jp},
    {-1}, {-1},
#line 357 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str869, ei_johab},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 270 "lib/aliases.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str921, ei_jisx0201}
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
