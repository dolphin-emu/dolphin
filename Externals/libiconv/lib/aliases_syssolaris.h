/* ANSI-C code produced by gperf version 3.2 */
/* Command-line: gperf -m 10 lib/aliases_syssolaris.gperf  */
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

#line 1 "lib/aliases_syssolaris.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 358
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 8
#define MAX_HASH_VALUE 1014
/* maximum key range = 1007, duplicates = 0 */

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
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015,   10,  212, 1015,   34,    2,
         8,   85,   31,    4,    3,  170,    6,    7,  285, 1015,
      1015, 1015, 1015, 1015, 1015,   26,  164,    2,   39,   78,
       190,   98,  118,    2,  168,  103,  149,  143,    4,    2,
       139, 1015,   40,   64,   28,  123,  106,  162,  284,    5,
         4, 1015, 1015, 1015, 1015,   82, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015,
      1015, 1015, 1015, 1015, 1015, 1015, 1015, 1015
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
    char stringpool_str8[sizeof("CN")];
    char stringpool_str12[sizeof("646")];
    char stringpool_str15[sizeof("866")];
    char stringpool_str19[sizeof("C99")];
    char stringpool_str22[sizeof("CP866")];
    char stringpool_str25[sizeof("862")];
    char stringpool_str26[sizeof("CP1251")];
    char stringpool_str28[sizeof("CP1256")];
    char stringpool_str29[sizeof("CP819")];
    char stringpool_str30[sizeof("CP1255")];
    char stringpool_str32[sizeof("CP862")];
    char stringpool_str34[sizeof("CP1258")];
    char stringpool_str38[sizeof("CP1252")];
    char stringpool_str39[sizeof("ASCII")];
    char stringpool_str46[sizeof("5601")];
    char stringpool_str48[sizeof("R8")];
    char stringpool_str50[sizeof("ISO8859-1")];
    char stringpool_str52[sizeof("ISO8859-6")];
    char stringpool_str53[sizeof("ISO8859-11")];
    char stringpool_str54[sizeof("ISO8859-5")];
    char stringpool_str55[sizeof("ISO8859-16")];
    char stringpool_str57[sizeof("ISO8859-15")];
    char stringpool_str58[sizeof("ISO8859-8")];
    char stringpool_str59[sizeof("CP949")];
    char stringpool_str60[sizeof("ISO8859-9")];
    char stringpool_str61[sizeof("ISO-8859-1")];
    char stringpool_str62[sizeof("ISO8859-2")];
    char stringpool_str63[sizeof("ISO-8859-6")];
    char stringpool_str64[sizeof("ISO-8859-11")];
    char stringpool_str65[sizeof("ISO-8859-5")];
    char stringpool_str66[sizeof("ISO-8859-16")];
    char stringpool_str67[sizeof("CP50221")];
    char stringpool_str68[sizeof("ISO-8859-15")];
    char stringpool_str69[sizeof("ISO-8859-8")];
    char stringpool_str70[sizeof("ISO646-CN")];
    char stringpool_str71[sizeof("ISO-8859-9")];
    char stringpool_str73[sizeof("ISO-8859-2")];
    char stringpool_str75[sizeof("CP154")];
    char stringpool_str77[sizeof("850")];
    char stringpool_str80[sizeof("ISO-IR-6")];
    char stringpool_str84[sizeof("CP1254")];
    char stringpool_str85[sizeof("CP850")];
    char stringpool_str86[sizeof("CP950")];
    char stringpool_str87[sizeof("ISO-IR-166")];
    char stringpool_str89[sizeof("ISO-IR-165")];
    char stringpool_str90[sizeof("CP1250")];
    char stringpool_str91[sizeof("ISO-IR-58")];
    char stringpool_str92[sizeof("ISO-IR-126")];
    char stringpool_str95[sizeof("EUCCN")];
    char stringpool_str96[sizeof("ISO-IR-159")];
    char stringpool_str98[sizeof("ISO-IR-226")];
    char stringpool_str99[sizeof("ISO-IR-199")];
    char stringpool_str101[sizeof("CP1131")];
    char stringpool_str102[sizeof("CP1361")];
    char stringpool_str103[sizeof("ISO-2022-CN")];
    char stringpool_str105[sizeof("CP936")];
    char stringpool_str106[sizeof("EUC-CN")];
    char stringpool_str107[sizeof("CSASCII")];
    char stringpool_str108[sizeof("ISO8859-4")];
    char stringpool_str111[sizeof("ISO8859-14")];
    char stringpool_str112[sizeof("CHAR")];
    char stringpool_str115[sizeof("CP932")];
    char stringpool_str116[sizeof("ISO-IR-101")];
    char stringpool_str117[sizeof("ISO8859-10")];
    char stringpool_str119[sizeof("ISO-8859-4")];
    char stringpool_str121[sizeof("ISO-IR-148")];
    char stringpool_str122[sizeof("ISO-8859-14")];
    char stringpool_str123[sizeof("ISO-IR-149")];
    char stringpool_str124[sizeof("HZ")];
    char stringpool_str125[sizeof("RK1048")];
    char stringpool_str126[sizeof("ISO-IR-109")];
    char stringpool_str128[sizeof("ISO-8859-10")];
    char stringpool_str129[sizeof("ANSI-1251")];
    char stringpool_str130[sizeof("UHC")];
    char stringpool_str131[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str133[sizeof("ISO_8859-1")];
    char stringpool_str135[sizeof("ISO_8859-6")];
    char stringpool_str136[sizeof("ISO_8859-11")];
    char stringpool_str137[sizeof("ISO_8859-5")];
    char stringpool_str138[sizeof("ISO_8859-16")];
    char stringpool_str139[sizeof("ISO-IR-14")];
    char stringpool_str140[sizeof("ISO_8859-15")];
    char stringpool_str141[sizeof("ISO_8859-8")];
    char stringpool_str142[sizeof("ISO_8859-16:2001")];
    char stringpool_str143[sizeof("ISO_8859-9")];
    char stringpool_str145[sizeof("ISO_8859-2")];
    char stringpool_str146[sizeof("TCVN")];
    char stringpool_str147[sizeof("ISO_8859-15:1998")];
    char stringpool_str148[sizeof("ISO-IR-110")];
    char stringpool_str149[sizeof("CSISO2022CN")];
    char stringpool_str150[sizeof("MAC")];
    char stringpool_str153[sizeof("L1")];
    char stringpool_str154[sizeof("L6")];
    char stringpool_str155[sizeof("L5")];
    char stringpool_str157[sizeof("L8")];
    char stringpool_str159[sizeof("L2")];
    char stringpool_str166[sizeof("IBM866")];
    char stringpool_str171[sizeof("ISO-IR-144")];
    char stringpool_str173[sizeof("IBM819")];
    char stringpool_str174[sizeof("ISO_8859-14:1998")];
    char stringpool_str175[sizeof("ISO-IR-138")];
    char stringpool_str176[sizeof("IBM862")];
    char stringpool_str177[sizeof("TIS620")];
    char stringpool_str179[sizeof("ISO_8859-10:1992")];
    char stringpool_str180[sizeof("ISO-IR-100")];
    char stringpool_str182[sizeof("L4")];
    char stringpool_str183[sizeof("KOI8-T")];
    char stringpool_str184[sizeof("VISCII")];
    char stringpool_str188[sizeof("TIS-620")];
    char stringpool_str189[sizeof("US")];
    char stringpool_str190[sizeof("CSVISCII")];
    char stringpool_str191[sizeof("ISO_8859-4")];
    char stringpool_str192[sizeof("CP1253")];
    char stringpool_str193[sizeof("LATIN1")];
    char stringpool_str194[sizeof("ISO_8859-14")];
    char stringpool_str195[sizeof("LATIN6")];
    char stringpool_str196[sizeof("CSKZ1048")];
    char stringpool_str197[sizeof("LATIN5")];
    char stringpool_str198[sizeof("SJIS")];
    char stringpool_str199[sizeof("KZ-1048")];
    char stringpool_str200[sizeof("ISO_8859-10")];
    char stringpool_str201[sizeof("LATIN8")];
    char stringpool_str202[sizeof("CSKOI8R")];
    char stringpool_str205[sizeof("LATIN2")];
    char stringpool_str207[sizeof("KOI8-R")];
    char stringpool_str212[sizeof("PT154")];
    char stringpool_str214[sizeof("LATIN-9")];
    char stringpool_str215[sizeof("GB2312")];
    char stringpool_str216[sizeof("ISO8859-3")];
    char stringpool_str218[sizeof("UCS-2")];
    char stringpool_str219[sizeof("ISO8859-13")];
    char stringpool_str220[sizeof("L10")];
    char stringpool_str223[sizeof("TIS620-0")];
    char stringpool_str225[sizeof("ELOT_928")];
    char stringpool_str227[sizeof("ISO-8859-3")];
    char stringpool_str228[sizeof("ARABIC")];
    char stringpool_str229[sizeof("IBM850")];
    char stringpool_str230[sizeof("ISO-8859-13")];
    char stringpool_str231[sizeof("ROMAN8")];
    char stringpool_str236[sizeof("L3")];
    char stringpool_str239[sizeof("US-ASCII")];
    char stringpool_str240[sizeof("KSC_5601")];
    char stringpool_str243[sizeof("ISO-10646-UCS-2")];
    char stringpool_str245[sizeof("CP874")];
    char stringpool_str246[sizeof("MS936")];
    char stringpool_str251[sizeof("LATIN4")];
    char stringpool_str258[sizeof("MS-ANSI")];
    char stringpool_str259[sizeof("CSUCS4")];
    char stringpool_str260[sizeof("LATIN10")];
    char stringpool_str261[sizeof("KOREAN")];
    char stringpool_str262[sizeof("ISO-IR-179")];
    char stringpool_str264[sizeof("UCS-4")];
    char stringpool_str266[sizeof("ISO-10646-UCS-4")];
    char stringpool_str267[sizeof("CP1133")];
    char stringpool_str268[sizeof("EUCKR")];
    char stringpool_str269[sizeof("CSUNICODE11")];
    char stringpool_str270[sizeof("ARMSCII-8")];
    char stringpool_str274[sizeof("BIG5")];
    char stringpool_str276[sizeof("ISO-2022-KR")];
    char stringpool_str279[sizeof("EUC-KR")];
    char stringpool_str280[sizeof("CSBIG5")];
    char stringpool_str281[sizeof("ECMA-118")];
    char stringpool_str282[sizeof("CNS11643")];
    char stringpool_str283[sizeof("UNICODE-1-1")];
    char stringpool_str285[sizeof("BIG-5")];
    char stringpool_str287[sizeof("ISO-CELTIC")];
    char stringpool_str288[sizeof("ISO-IR-203")];
    char stringpool_str290[sizeof("MACCROATIAN")];
    char stringpool_str291[sizeof("CN-BIG5")];
    char stringpool_str294[sizeof("CSISOLATIN1")];
    char stringpool_str295[sizeof("GB_1988-80")];
    char stringpool_str296[sizeof("CSISOLATIN6")];
    char stringpool_str298[sizeof("CSISOLATIN5")];
    char stringpool_str299[sizeof("ISO_8859-3")];
    char stringpool_str300[sizeof("GB18030")];
    char stringpool_str301[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str302[sizeof("ISO_8859-13")];
    char stringpool_str306[sizeof("CSISOLATIN2")];
    char stringpool_str307[sizeof("GBK")];
    char stringpool_str309[sizeof("JP")];
    char stringpool_str311[sizeof("ISO646-US")];
    char stringpool_str313[sizeof("CHINESE")];
    char stringpool_str315[sizeof("X0212")];
    char stringpool_str321[sizeof("L7")];
    char stringpool_str322[sizeof("CSISO2022KR")];
    char stringpool_str323[sizeof("CSISOLATINARABIC")];
    char stringpool_str327[sizeof("JIS0208")];
    char stringpool_str328[sizeof("MACTHAI")];
    char stringpool_str329[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str330[sizeof("JAVA")];
    char stringpool_str331[sizeof("ECMA-114")];
    char stringpool_str334[sizeof("CSIBM866")];
    char stringpool_str335[sizeof("X0201")];
    char stringpool_str337[sizeof("UTF-16")];
    char stringpool_str339[sizeof("CSUNICODE")];
    char stringpool_str340[sizeof("UTF-8")];
    char stringpool_str343[sizeof("X0208")];
    char stringpool_str345[sizeof("KS_C_5601-1989")];
    char stringpool_str346[sizeof("TCVN5712-1")];
    char stringpool_str348[sizeof("PCK")];
    char stringpool_str349[sizeof("TCVN-5712")];
    char stringpool_str352[sizeof("CSISOLATIN4")];
    char stringpool_str353[sizeof("STRK1048-2002")];
    char stringpool_str355[sizeof("PTCP154")];
    char stringpool_str356[sizeof("CYRILLIC")];
    char stringpool_str358[sizeof("CSISO14JISC6220RO")];
    char stringpool_str359[sizeof("LATIN3")];
    char stringpool_str362[sizeof("CP1257")];
    char stringpool_str364[sizeof("HP-ROMAN8")];
    char stringpool_str371[sizeof("CSISO159JISX02121990")];
    char stringpool_str372[sizeof("MACROMAN")];
    char stringpool_str373[sizeof("KOI8-U")];
    char stringpool_str375[sizeof("GREEK8")];
    char stringpool_str377[sizeof("GB_2312-80")];
    char stringpool_str383[sizeof("CSGB2312")];
    char stringpool_str386[sizeof("ISO8859-7")];
    char stringpool_str387[sizeof("CSPTCP154")];
    char stringpool_str391[sizeof("TIS620.2529-1")];
    char stringpool_str392[sizeof("MS-EE")];
    char stringpool_str395[sizeof("CSEUCKR")];
    char stringpool_str397[sizeof("ISO-8859-7")];
    char stringpool_str400[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str404[sizeof("ISO-2022-JP-1")];
    char stringpool_str405[sizeof("ASMO-708")];
    char stringpool_str406[sizeof("CSKSC56011987")];
    char stringpool_str410[sizeof("ISO-2022-JP-2")];
    char stringpool_str412[sizeof("CN-GB-ISOIR165")];
    char stringpool_str414[sizeof("KOI8-RU")];
    char stringpool_str416[sizeof("MACARABIC")];
    char stringpool_str417[sizeof("VISCII1.1-1")];
    char stringpool_str419[sizeof("ISO-IR-57")];
    char stringpool_str421[sizeof("ISO-IR-87")];
    char stringpool_str422[sizeof("ISO-IR-157")];
    char stringpool_str424[sizeof("MACROMANIA")];
    char stringpool_str426[sizeof("ISO-IR-127")];
    char stringpool_str429[sizeof("ISO_8859-5:1988")];
    char stringpool_str430[sizeof("UTF-32")];
    char stringpool_str431[sizeof("ISO_8859-8:1988")];
    char stringpool_str433[sizeof("ISO_8859-9:1989")];
    char stringpool_str435[sizeof("CP367")];
    char stringpool_str437[sizeof("EUCTW")];
    char stringpool_str441[sizeof("CSUNICODE11UTF7")];
    char stringpool_str443[sizeof("CN-GB")];
    char stringpool_str448[sizeof("EUC-TW")];
    char stringpool_str449[sizeof("JIS_C6226-1983")];
    char stringpool_str455[sizeof("CSISO2022JP2")];
    char stringpool_str456[sizeof("ISO_8859-4:1988")];
    char stringpool_str457[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str460[sizeof("CSISOLATIN3")];
    char stringpool_str464[sizeof("CYRILLIC-ASIAN")];
    char stringpool_str465[sizeof("GREEK")];
    char stringpool_str467[sizeof("ISO-2022-JP-MS")];
    char stringpool_str469[sizeof("ISO_8859-7")];
    char stringpool_str471[sizeof("WINDOWS-1251")];
    char stringpool_str472[sizeof("WINDOWS-1256")];
    char stringpool_str473[sizeof("WINDOWS-1255")];
    char stringpool_str475[sizeof("WINDOWS-1258")];
    char stringpool_str477[sizeof("WINDOWS-1252")];
    char stringpool_str484[sizeof("CSISO58GB231280")];
    char stringpool_str490[sizeof("MACINTOSH")];
    char stringpool_str491[sizeof("WCHAR_T")];
    char stringpool_str494[sizeof("MACICELAND")];
    char stringpool_str495[sizeof("CSISOLATINGREEK")];
    char stringpool_str496[sizeof("CSHPROMAN8")];
    char stringpool_str500[sizeof("WINDOWS-1254")];
    char stringpool_str501[sizeof("ISO_646.IRV:1991")];
    char stringpool_str503[sizeof("WINDOWS-1250")];
    char stringpool_str505[sizeof("MS-CYRL")];
    char stringpool_str506[sizeof("ISO646-JP")];
    char stringpool_str508[sizeof("KS_C_5601-1987")];
    char stringpool_str509[sizeof("MACCYRILLIC")];
    char stringpool_str510[sizeof("ISO_8859-3:1988")];
    char stringpool_str517[sizeof("UCS-2LE")];
    char stringpool_str520[sizeof("CSMACINTOSH")];
    char stringpool_str521[sizeof("HZ-GB-2312")];
    char stringpool_str529[sizeof("LATIN7")];
    char stringpool_str530[sizeof("UCS-2-INTERNAL")];
    char stringpool_str531[sizeof("EUCJP")];
    char stringpool_str532[sizeof("UCS-2BE")];
    char stringpool_str534[sizeof("CSISO57GB1988")];
    char stringpool_str538[sizeof("MS_KANJI")];
    char stringpool_str539[sizeof("ISO-2022-JP")];
    char stringpool_str540[sizeof("UCS-4LE")];
    char stringpool_str542[sizeof("EUC-JP")];
    char stringpool_str544[sizeof("UNICODELITTLE")];
    char stringpool_str545[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str546[sizeof("TIS620.2533-1")];
    char stringpool_str552[sizeof("WINDOWS-936")];
    char stringpool_str553[sizeof("UCS-4-INTERNAL")];
    char stringpool_str554[sizeof("WINDOWS-1253")];
    char stringpool_str555[sizeof("UCS-4BE")];
    char stringpool_str557[sizeof("MS-TURK")];
    char stringpool_str558[sizeof("GEORGIAN-PS")];
    char stringpool_str564[sizeof("CSEUCTW")];
    char stringpool_str565[sizeof("IBM-CP1133")];
    char stringpool_str570[sizeof("MULELAO-1")];
    char stringpool_str575[sizeof("CSISOLATINHEBREW")];
    char stringpool_str578[sizeof("TIS620.2533-0")];
    char stringpool_str579[sizeof("IBM367")];
    char stringpool_str580[sizeof("MS-ARAB")];
    char stringpool_str585[sizeof("CSISO2022JP")];
    char stringpool_str591[sizeof("ISO_8859-1:1987")];
    char stringpool_str592[sizeof("ISO_8859-6:1987")];
    char stringpool_str597[sizeof("ISO_8859-2:1987")];
    char stringpool_str600[sizeof("MS-HEBR")];
    char stringpool_str601[sizeof("SHIFT-JIS")];
    char stringpool_str609[sizeof("MACUKRAINE")];
    char stringpool_str614[sizeof("GB18030:2022")];
    char stringpool_str618[sizeof("UNICODEBIG")];
    char stringpool_str627[sizeof("TIS620.2533")];
    char stringpool_str629[sizeof("JISX0201-1976")];
    char stringpool_str636[sizeof("GB18030:2005")];
    char stringpool_str639[sizeof("WINDOWS-1257")];
    char stringpool_str641[sizeof("UTF-16LE")];
    char stringpool_str644[sizeof("MACCENTRALEUROPE")];
    char stringpool_str645[sizeof("JOHAB")];
    char stringpool_str653[sizeof("MACGREEK")];
    char stringpool_str656[sizeof("UTF-16BE")];
    char stringpool_str661[sizeof("MS-GREEK")];
    char stringpool_str667[sizeof("JIS_X0212")];
    char stringpool_str668[sizeof("UTF-7")];
    char stringpool_str673[sizeof("SHIFT_JIS")];
    char stringpool_str674[sizeof("ISO_8859-7:2003")];
    char stringpool_str676[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str687[sizeof("JIS_X0201")];
    char stringpool_str690[sizeof("BIG5HKSCS")];
    char stringpool_str692[sizeof("WINDOWS-874")];
    char stringpool_str695[sizeof("JIS_X0208")];
    char stringpool_str701[sizeof("BIG5-HKSCS")];
    char stringpool_str710[sizeof("JIS_X0212-1990")];
    char stringpool_str712[sizeof("CSSHIFTJIS")];
    char stringpool_str719[sizeof("TCVN5712-1:1993")];
    char stringpool_str723[sizeof("BIGFIVE")];
    char stringpool_str729[sizeof("UTF-32LE")];
    char stringpool_str730[sizeof("HEBREW")];
    char stringpool_str734[sizeof("BIG-FIVE")];
    char stringpool_str740[sizeof("JIS_X0208-1990")];
    char stringpool_str744[sizeof("UTF-32BE")];
    char stringpool_str751[sizeof("MACTURKISH")];
    char stringpool_str759[sizeof("ISO_8859-7:1987")];
    char stringpool_str772[sizeof("NEXTSTEP")];
    char stringpool_str785[sizeof("CSISO87JISX0208")];
    char stringpool_str791[sizeof("JIS_X0208-1983")];
    char stringpool_str797[sizeof("UCS-2-SWAPPED")];
    char stringpool_str798[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str815[sizeof("ANSI_X3.4-1986")];
    char stringpool_str818[sizeof("ANSI_X3.4-1968")];
    char stringpool_str820[sizeof("UCS-4-SWAPPED")];
    char stringpool_str871[sizeof("WINBALTRIM")];
    char stringpool_str879[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str914[sizeof("JIS_X0212.1990-0")];
    char stringpool_str929[sizeof("BIG5-HKSCS:2001")];
    char stringpool_str933[sizeof("BIG5-HKSCS:2008")];
    char stringpool_str934[sizeof("BIG5-HKSCS:1999")];
    char stringpool_str944[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str956[sizeof("MACHEBREW")];
    char stringpool_str958[sizeof("BIG5-HKSCS:2004")];
    char stringpool_str1014[sizeof("KO_KR.JOHAP92")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "CN",
    "646",
    "866",
    "C99",
    "CP866",
    "862",
    "CP1251",
    "CP1256",
    "CP819",
    "CP1255",
    "CP862",
    "CP1258",
    "CP1252",
    "ASCII",
    "5601",
    "R8",
    "ISO8859-1",
    "ISO8859-6",
    "ISO8859-11",
    "ISO8859-5",
    "ISO8859-16",
    "ISO8859-15",
    "ISO8859-8",
    "CP949",
    "ISO8859-9",
    "ISO-8859-1",
    "ISO8859-2",
    "ISO-8859-6",
    "ISO-8859-11",
    "ISO-8859-5",
    "ISO-8859-16",
    "CP50221",
    "ISO-8859-15",
    "ISO-8859-8",
    "ISO646-CN",
    "ISO-8859-9",
    "ISO-8859-2",
    "CP154",
    "850",
    "ISO-IR-6",
    "CP1254",
    "CP850",
    "CP950",
    "ISO-IR-166",
    "ISO-IR-165",
    "CP1250",
    "ISO-IR-58",
    "ISO-IR-126",
    "EUCCN",
    "ISO-IR-159",
    "ISO-IR-226",
    "ISO-IR-199",
    "CP1131",
    "CP1361",
    "ISO-2022-CN",
    "CP936",
    "EUC-CN",
    "CSASCII",
    "ISO8859-4",
    "ISO8859-14",
    "CHAR",
    "CP932",
    "ISO-IR-101",
    "ISO8859-10",
    "ISO-8859-4",
    "ISO-IR-148",
    "ISO-8859-14",
    "ISO-IR-149",
    "HZ",
    "RK1048",
    "ISO-IR-109",
    "ISO-8859-10",
    "ANSI-1251",
    "UHC",
    "ISO-2022-CN-EXT",
    "ISO_8859-1",
    "ISO_8859-6",
    "ISO_8859-11",
    "ISO_8859-5",
    "ISO_8859-16",
    "ISO-IR-14",
    "ISO_8859-15",
    "ISO_8859-8",
    "ISO_8859-16:2001",
    "ISO_8859-9",
    "ISO_8859-2",
    "TCVN",
    "ISO_8859-15:1998",
    "ISO-IR-110",
    "CSISO2022CN",
    "MAC",
    "L1",
    "L6",
    "L5",
    "L8",
    "L2",
    "IBM866",
    "ISO-IR-144",
    "IBM819",
    "ISO_8859-14:1998",
    "ISO-IR-138",
    "IBM862",
    "TIS620",
    "ISO_8859-10:1992",
    "ISO-IR-100",
    "L4",
    "KOI8-T",
    "VISCII",
    "TIS-620",
    "US",
    "CSVISCII",
    "ISO_8859-4",
    "CP1253",
    "LATIN1",
    "ISO_8859-14",
    "LATIN6",
    "CSKZ1048",
    "LATIN5",
    "SJIS",
    "KZ-1048",
    "ISO_8859-10",
    "LATIN8",
    "CSKOI8R",
    "LATIN2",
    "KOI8-R",
    "PT154",
    "LATIN-9",
    "GB2312",
    "ISO8859-3",
    "UCS-2",
    "ISO8859-13",
    "L10",
    "TIS620-0",
    "ELOT_928",
    "ISO-8859-3",
    "ARABIC",
    "IBM850",
    "ISO-8859-13",
    "ROMAN8",
    "L3",
    "US-ASCII",
    "KSC_5601",
    "ISO-10646-UCS-2",
    "CP874",
    "MS936",
    "LATIN4",
    "MS-ANSI",
    "CSUCS4",
    "LATIN10",
    "KOREAN",
    "ISO-IR-179",
    "UCS-4",
    "ISO-10646-UCS-4",
    "CP1133",
    "EUCKR",
    "CSUNICODE11",
    "ARMSCII-8",
    "BIG5",
    "ISO-2022-KR",
    "EUC-KR",
    "CSBIG5",
    "ECMA-118",
    "CNS11643",
    "UNICODE-1-1",
    "BIG-5",
    "ISO-CELTIC",
    "ISO-IR-203",
    "MACCROATIAN",
    "CN-BIG5",
    "CSISOLATIN1",
    "GB_1988-80",
    "CSISOLATIN6",
    "CSISOLATIN5",
    "ISO_8859-3",
    "GB18030",
    "CSISOLATINCYRILLIC",
    "ISO_8859-13",
    "CSISOLATIN2",
    "GBK",
    "JP",
    "ISO646-US",
    "CHINESE",
    "X0212",
    "L7",
    "CSISO2022KR",
    "CSISOLATINARABIC",
    "JIS0208",
    "MACTHAI",
    "GEORGIAN-ACADEMY",
    "JAVA",
    "ECMA-114",
    "CSIBM866",
    "X0201",
    "UTF-16",
    "CSUNICODE",
    "UTF-8",
    "X0208",
    "KS_C_5601-1989",
    "TCVN5712-1",
    "PCK",
    "TCVN-5712",
    "CSISOLATIN4",
    "STRK1048-2002",
    "PTCP154",
    "CYRILLIC",
    "CSISO14JISC6220RO",
    "LATIN3",
    "CP1257",
    "HP-ROMAN8",
    "CSISO159JISX02121990",
    "MACROMAN",
    "KOI8-U",
    "GREEK8",
    "GB_2312-80",
    "CSGB2312",
    "ISO8859-7",
    "CSPTCP154",
    "TIS620.2529-1",
    "MS-EE",
    "CSEUCKR",
    "ISO-8859-7",
    "JIS_C6220-1969-RO",
    "ISO-2022-JP-1",
    "ASMO-708",
    "CSKSC56011987",
    "ISO-2022-JP-2",
    "CN-GB-ISOIR165",
    "KOI8-RU",
    "MACARABIC",
    "VISCII1.1-1",
    "ISO-IR-57",
    "ISO-IR-87",
    "ISO-IR-157",
    "MACROMANIA",
    "ISO-IR-127",
    "ISO_8859-5:1988",
    "UTF-32",
    "ISO_8859-8:1988",
    "ISO_8859-9:1989",
    "CP367",
    "EUCTW",
    "CSUNICODE11UTF7",
    "CN-GB",
    "EUC-TW",
    "JIS_C6226-1983",
    "CSISO2022JP2",
    "ISO_8859-4:1988",
    "UNICODE-1-1-UTF-7",
    "CSISOLATIN3",
    "CYRILLIC-ASIAN",
    "GREEK",
    "ISO-2022-JP-MS",
    "ISO_8859-7",
    "WINDOWS-1251",
    "WINDOWS-1256",
    "WINDOWS-1255",
    "WINDOWS-1258",
    "WINDOWS-1252",
    "CSISO58GB231280",
    "MACINTOSH",
    "WCHAR_T",
    "MACICELAND",
    "CSISOLATINGREEK",
    "CSHPROMAN8",
    "WINDOWS-1254",
    "ISO_646.IRV:1991",
    "WINDOWS-1250",
    "MS-CYRL",
    "ISO646-JP",
    "KS_C_5601-1987",
    "MACCYRILLIC",
    "ISO_8859-3:1988",
    "UCS-2LE",
    "CSMACINTOSH",
    "HZ-GB-2312",
    "LATIN7",
    "UCS-2-INTERNAL",
    "EUCJP",
    "UCS-2BE",
    "CSISO57GB1988",
    "MS_KANJI",
    "ISO-2022-JP",
    "UCS-4LE",
    "EUC-JP",
    "UNICODELITTLE",
    "CSPC862LATINHEBREW",
    "TIS620.2533-1",
    "WINDOWS-936",
    "UCS-4-INTERNAL",
    "WINDOWS-1253",
    "UCS-4BE",
    "MS-TURK",
    "GEORGIAN-PS",
    "CSEUCTW",
    "IBM-CP1133",
    "MULELAO-1",
    "CSISOLATINHEBREW",
    "TIS620.2533-0",
    "IBM367",
    "MS-ARAB",
    "CSISO2022JP",
    "ISO_8859-1:1987",
    "ISO_8859-6:1987",
    "ISO_8859-2:1987",
    "MS-HEBR",
    "SHIFT-JIS",
    "MACUKRAINE",
    "GB18030:2022",
    "UNICODEBIG",
    "TIS620.2533",
    "JISX0201-1976",
    "GB18030:2005",
    "WINDOWS-1257",
    "UTF-16LE",
    "MACCENTRALEUROPE",
    "JOHAB",
    "MACGREEK",
    "UTF-16BE",
    "MS-GREEK",
    "JIS_X0212",
    "UTF-7",
    "SHIFT_JIS",
    "ISO_8859-7:2003",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "JIS_X0201",
    "BIG5HKSCS",
    "WINDOWS-874",
    "JIS_X0208",
    "BIG5-HKSCS",
    "JIS_X0212-1990",
    "CSSHIFTJIS",
    "TCVN5712-1:1993",
    "BIGFIVE",
    "UTF-32LE",
    "HEBREW",
    "BIG-FIVE",
    "JIS_X0208-1990",
    "UTF-32BE",
    "MACTURKISH",
    "ISO_8859-7:1987",
    "NEXTSTEP",
    "CSISO87JISX0208",
    "JIS_X0208-1983",
    "UCS-2-SWAPPED",
    "CSPC850MULTILINGUAL",
    "ANSI_X3.4-1986",
    "ANSI_X3.4-1968",
    "UCS-4-SWAPPED",
    "WINBALTRIM",
    "CSHALFWIDTHKATAKANA",
    "JIS_X0212.1990-0",
    "BIG5-HKSCS:2001",
    "BIG5-HKSCS:2008",
    "BIG5-HKSCS:1999",
    "CSEUCPKDFMTJAPANESE",
    "MACHEBREW",
    "BIG5-HKSCS:2004",
    "KO_KR.JOHAP92"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 291 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str8, ei_iso646_cn},
    {-1}, {-1}, {-1},
#line 23 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str12, ei_ascii},
    {-1}, {-1},
#line 209 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15, ei_cp866},
    {-1}, {-1}, {-1},
#line 52 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str19, ei_c99},
    {-1}, {-1},
#line 207 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22, ei_cp866},
    {-1}, {-1},
#line 205 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25, ei_cp862},
#line 175 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str26, ei_cp1251},
    {-1},
#line 191 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str28, ei_cp1256},
#line 58 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29, ei_iso8859_1},
#line 188 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30, ei_cp1255},
    {-1},
#line 203 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str32, ei_cp862},
    {-1},
#line 197 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str34, ei_cp1258},
    {-1}, {-1}, {-1},
#line 179 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str38, ei_cp1252},
#line 13 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str39, ei_ascii},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 360 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str46, ei_euc_kr},
    {-1},
#line 229 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48, ei_hp_roman8},
    {-1},
#line 63 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str50, ei_iso8859_1},
    {-1},
#line 103 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str52, ei_iso8859_6},
#line 140 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str53, ei_iso8859_11},
#line 94 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str54, ei_iso8859_5},
#line 167 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55, ei_iso8859_16},
    {-1},
#line 160 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str57, ei_iso8859_15},
#line 121 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str58, ei_iso8859_8},
#line 361 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str59, ei_cp949},
#line 129 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str60, ei_iso8859_9},
#line 54 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str61, ei_iso8859_1},
#line 71 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str62, ei_iso8859_2},
#line 95 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str63, ei_iso8859_6},
#line 138 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64, ei_iso8859_11},
#line 88 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65, ei_iso8859_5},
#line 161 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str66, ei_iso8859_16},
#line 322 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67, ei_iso2022_jpms},
#line 155 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str68, ei_iso8859_15},
#line 115 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str69, ei_iso8859_8},
#line 289 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str70, ei_iso646_cn},
#line 122 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71, ei_iso8859_9},
    {-1},
#line 64 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str73, ei_iso8859_2},
    {-1},
#line 238 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str75, ei_pt154},
    {-1},
#line 201 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str77, ei_cp850},
    {-1}, {-1},
#line 16 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80, ei_ascii},
    {-1}, {-1}, {-1},
#line 185 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str84, ei_cp1254},
#line 199 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str85, ei_cp850},
#line 350 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str86, ei_cp950},
#line 254 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str87, ei_tis620},
    {-1},
#line 297 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str89, ei_isoir165},
#line 172 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str90, ei_cp1250},
#line 294 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str91, ei_gb2312},
#line 108 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str92, ei_iso8859_7},
    {-1}, {-1},
#line 324 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95, ei_euc_cn},
#line 286 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96, ei_jisx0212},
    {-1},
#line 164 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str98, ei_iso8859_16},
#line 150 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str99, ei_iso8859_14},
    {-1},
#line 211 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str101, ei_cp1131},
#line 364 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str102, ei_johab},
#line 335 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str103, ei_iso2022_cn},
    {-1},
#line 329 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str105, ei_cp936},
#line 323 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str106, ei_euc_cn},
#line 22 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str107, ei_ascii},
#line 87 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108, ei_iso8859_4},
    {-1}, {-1},
#line 154 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111, ei_iso8859_14},
#line 368 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str112, ei_local_char},
    {-1}, {-1},
#line 315 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str115, ei_cp932},
#line 67 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str116, ei_iso8859_2},
#line 137 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str117, ei_iso8859_10},
    {-1},
#line 80 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str119, ei_iso8859_4},
    {-1},
#line 125 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str121, ei_iso8859_9},
#line 147 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str122, ei_iso8859_14},
#line 302 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str123, ei_ksc5601},
#line 338 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124, ei_hz},
#line 241 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125, ei_rk1048},
#line 75 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str126, ei_iso8859_3},
    {-1},
#line 130 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128, ei_iso8859_10},
#line 178 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str129, ei_cp1251},
#line 362 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str130, ei_cp949},
#line 337 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131, ei_iso2022_cn_ext},
    {-1},
#line 55 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str133, ei_iso8859_1},
    {-1},
#line 96 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str135, ei_iso8859_6},
#line 139 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136, ei_iso8859_11},
#line 89 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str137, ei_iso8859_5},
#line 162 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str138, ei_iso8859_16},
#line 267 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str139, ei_iso646_jp},
#line 156 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str140, ei_iso8859_15},
#line 116 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str141, ei_iso8859_8},
#line 163 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str142, ei_iso8859_16},
#line 123 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str143, ei_iso8859_9},
    {-1},
#line 65 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145, ei_iso8859_2},
#line 261 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146, ei_tcvn},
#line 157 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str147, ei_iso8859_15},
#line 83 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str148, ei_iso8859_4},
#line 336 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149, ei_iso2022_cn},
#line 214 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150, ei_mac_roman},
    {-1}, {-1},
#line 61 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str153, ei_iso8859_1},
#line 135 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154, ei_iso8859_10},
#line 127 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155, ei_iso8859_9},
    {-1},
#line 152 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157, ei_iso8859_14},
    {-1},
#line 69 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159, ei_iso8859_2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 208 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str166, ei_cp866},
    {-1}, {-1}, {-1}, {-1},
#line 91 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171, ei_iso8859_5},
    {-1},
#line 59 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173, ei_iso8859_1},
#line 149 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str174, ei_iso8859_14},
#line 118 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str175, ei_iso8859_8},
#line 204 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176, ei_cp862},
#line 249 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177, ei_tis620},
    {-1},
#line 132 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_10},
#line 57 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180, ei_iso8859_1},
    {-1},
#line 85 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182, ei_iso8859_4},
#line 235 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183, ei_koi8_t},
#line 258 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184, ei_viscii},
    {-1}, {-1}, {-1},
#line 248 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str188, ei_tis620},
#line 21 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str189, ei_ascii},
#line 260 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190, ei_viscii},
#line 81 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191, ei_iso8859_4},
#line 182 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192, ei_cp1253},
#line 60 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193, ei_iso8859_1},
#line 148 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str194, ei_iso8859_14},
#line 134 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str195, ei_iso8859_10},
#line 244 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196, ei_rk1048},
#line 126 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197, ei_iso8859_9},
#line 311 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str198, ei_sjis},
#line 243 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199, ei_rk1048},
#line 131 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str200, ei_iso8859_10},
#line 151 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201, ei_iso8859_14},
#line 169 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202, ei_koi8_r},
    {-1}, {-1},
#line 68 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205, ei_iso8859_2},
    {-1},
#line 168 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207, ei_koi8_r},
    {-1}, {-1}, {-1}, {-1},
#line 236 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212, ei_pt154},
    {-1},
#line 159 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str214, ei_iso8859_15},
#line 325 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str215, ei_euc_cn},
#line 79 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str216, ei_iso8859_3},
    {-1},
#line 25 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str218, ei_ucs2},
#line 146 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219, ei_iso8859_13},
#line 166 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220, ei_iso8859_16},
    {-1}, {-1},
#line 250 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str223, ei_tis620},
    {-1},
#line 110 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225, ei_iso8859_7},
    {-1},
#line 72 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str227, ei_iso8859_3},
#line 101 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228, ei_iso8859_6},
#line 200 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229, ei_cp850},
#line 141 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str230, ei_iso8859_13},
#line 228 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1},
#line 77 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236, ei_iso8859_3},
    {-1}, {-1},
#line 12 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239, ei_ascii},
#line 299 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240, ei_ksc5601},
    {-1}, {-1},
#line 26 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243, ei_ucs2},
    {-1},
#line 256 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str245, ei_cp874},
#line 330 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246, ei_cp936},
    {-1}, {-1}, {-1}, {-1},
#line 84 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str251, ei_iso8859_4},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 181 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str258, ei_cp1252},
#line 36 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str259, ei_ucs4},
#line 165 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260, ei_iso8859_16},
#line 304 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261, ei_ksc5601},
#line 143 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str262, ei_iso8859_13},
    {-1},
#line 34 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264, ei_ucs4},
    {-1},
#line 35 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str266, ei_ucs4},
#line 246 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267, ei_cp1133},
#line 358 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268, ei_euc_kr},
#line 31 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str269, ei_ucs2be},
#line 232 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270, ei_armscii_8},
    {-1}, {-1}, {-1},
#line 344 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274, ei_ces_big5},
    {-1},
#line 366 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276, ei_iso2022_kr},
    {-1}, {-1},
#line 357 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str279, ei_euc_kr},
#line 349 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280, ei_ces_big5},
#line 109 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281, ei_iso8859_7},
#line 343 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282, ei_euc_tw},
#line 30 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str283, ei_ucs2be},
    {-1},
#line 345 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285, ei_ces_big5},
    {-1},
#line 153 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287, ei_iso8859_14},
#line 158 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288, ei_iso8859_15},
    {-1},
#line 218 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290, ei_mac_croatian},
#line 348 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str291, ei_ces_big5},
    {-1}, {-1},
#line 62 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str294, ei_iso8859_1},
#line 288 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295, ei_iso646_cn},
#line 136 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str296, ei_iso8859_10},
    {-1},
#line 128 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str298, ei_iso8859_9},
#line 73 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str299, ei_iso8859_3},
#line 332 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str300, ei_gb18030_2005},
#line 93 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301, ei_iso8859_5},
#line 142 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302, ei_iso8859_13},
    {-1}, {-1}, {-1},
#line 70 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306, ei_iso8859_2},
#line 328 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307, ei_ces_gbk},
    {-1},
#line 268 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str309, ei_iso646_jp},
    {-1},
#line 14 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str311, ei_ascii},
    {-1},
#line 296 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str313, ei_gb2312},
    {-1},
#line 285 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str315, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 145 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321, ei_iso8859_13},
#line 367 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322, ei_iso2022_kr},
#line 102 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str323, ei_iso8859_6},
    {-1}, {-1}, {-1},
#line 277 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327, ei_jisx0208},
#line 226 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str328, ei_mac_thai},
#line 233 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str329, ei_georgian_academy},
#line 53 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str330, ei_java},
#line 99 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str331, ei_iso8859_6},
    {-1}, {-1},
#line 210 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str334, ei_cp866},
#line 272 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str335, ei_jisx0201},
    {-1},
#line 39 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str337, ei_utf16},
    {-1},
#line 27 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str339, ei_ucs2},
#line 24 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str340, ei_utf8},
    {-1}, {-1},
#line 278 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str343, ei_jisx0208},
    {-1},
#line 301 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str345, ei_ksc5601},
#line 263 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str346, ei_tcvn},
    {-1},
#line 314 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str348, ei_sjis},
#line 262 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str349, ei_tcvn},
    {-1}, {-1},
#line 86 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352, ei_iso8859_4},
#line 242 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str353, ei_rk1048},
    {-1},
#line 237 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str355, ei_pt154},
#line 92 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str356, ei_iso8859_5},
    {-1},
#line 269 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str358, ei_iso646_jp},
#line 76 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str359, ei_iso8859_3},
    {-1}, {-1},
#line 194 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362, ei_cp1257},
    {-1},
#line 227 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 287 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str371, ei_jisx0212},
#line 212 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372, ei_mac_roman},
#line 170 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str373, ei_koi8_u},
    {-1},
#line 111 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375, ei_iso8859_7},
    {-1},
#line 293 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str377, ei_gb2312},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 327 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str383, ei_euc_cn},
    {-1}, {-1},
#line 114 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386, ei_iso8859_7},
#line 240 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str387, ei_pt154},
    {-1}, {-1}, {-1},
#line 251 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391, ei_tis620},
#line 174 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392, ei_cp1250},
    {-1}, {-1},
#line 359 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str395, ei_euc_kr},
    {-1},
#line 104 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str397, ei_iso8859_7},
    {-1}, {-1},
#line 265 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str400, ei_iso646_jp},
    {-1}, {-1}, {-1},
#line 318 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str404, ei_iso2022_jp1},
#line 100 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str405, ei_iso8859_6},
#line 303 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str406, ei_ksc5601},
    {-1}, {-1}, {-1},
#line 319 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str410, ei_iso2022_jp2},
    {-1},
#line 298 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str412, ei_isoir165},
    {-1},
#line 171 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str414, ei_koi8_ru},
    {-1},
#line 225 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416, ei_mac_arabic},
#line 259 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str417, ei_viscii},
    {-1},
#line 290 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str419, ei_iso646_cn},
    {-1},
#line 279 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421, ei_jisx0208},
#line 133 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str422, ei_iso8859_10},
    {-1},
#line 219 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424, ei_mac_romania},
    {-1},
#line 98 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426, ei_iso8859_6},
    {-1}, {-1},
#line 90 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429, ei_iso8859_5},
#line 42 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430, ei_utf32},
#line 117 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str431, ei_iso8859_8},
    {-1},
#line 124 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str433, ei_iso8859_9},
    {-1},
#line 19 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str435, ei_ascii},
    {-1},
#line 341 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437, ei_euc_tw},
    {-1}, {-1}, {-1},
#line 47 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str441, ei_utf7},
    {-1},
#line 326 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str443, ei_euc_cn},
    {-1}, {-1}, {-1}, {-1},
#line 340 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str448, ei_euc_tw},
#line 280 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str449, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 320 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str455, ei_iso2022_jp2},
#line 82 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str456, ei_iso8859_4},
#line 46 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457, ei_utf7},
    {-1}, {-1},
#line 78 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460, ei_iso8859_3},
    {-1}, {-1}, {-1},
#line 239 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str464, ei_pt154},
#line 112 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str465, ei_iso8859_7},
    {-1},
#line 321 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467, ei_iso2022_jpms},
    {-1},
#line 105 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469, ei_iso8859_7},
    {-1},
#line 176 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str471, ei_cp1251},
#line 192 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str472, ei_cp1256},
#line 189 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str473, ei_cp1255},
    {-1},
#line 198 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str475, ei_cp1258},
    {-1},
#line 180 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477, ei_cp1252},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 295 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str484, ei_gb2312},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 213 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490, ei_mac_roman},
#line 369 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str491, ei_local_wchar_t},
    {-1}, {-1},
#line 217 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str494, ei_mac_iceland},
#line 113 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495, ei_iso8859_7},
#line 230 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str496, ei_hp_roman8},
    {-1}, {-1}, {-1},
#line 186 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str500, ei_cp1254},
#line 15 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str501, ei_ascii},
    {-1},
#line 173 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str503, ei_cp1250},
    {-1},
#line 177 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str505, ei_cp1251},
#line 266 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str506, ei_iso646_jp},
    {-1},
#line 300 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str508, ei_ksc5601},
#line 220 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str509, ei_mac_cyrillic},
#line 74 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str510, ei_iso8859_3},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 32 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str517, ei_ucs2le},
    {-1}, {-1},
#line 215 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str520, ei_mac_roman},
#line 339 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str521, ei_hz},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 144 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str529, ei_iso8859_13},
#line 48 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str530, ei_ucs2internal},
#line 306 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str531, ei_euc_jp},
#line 28 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str532, ei_ucs2be},
    {-1},
#line 292 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str534, ei_iso646_cn},
    {-1}, {-1}, {-1},
#line 312 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str538, ei_sjis},
#line 316 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str539, ei_iso2022_jp},
#line 38 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str540, ei_ucs4le},
    {-1},
#line 305 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str542, ei_euc_jp},
    {-1},
#line 33 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str544, ei_ucs2le},
#line 206 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str545, ei_cp862},
#line 253 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str546, ei_tis620},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 331 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str552, ei_cp936},
#line 50 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str553, ei_ucs4internal},
#line 183 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str554, ei_cp1253},
#line 37 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str555, ei_ucs4be},
    {-1},
#line 187 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str557, ei_cp1254},
#line 234 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str558, ei_georgian_ps},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 342 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str564, ei_euc_tw},
#line 247 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str565, ei_cp1133},
    {-1}, {-1}, {-1}, {-1},
#line 245 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str570, ei_mulelao},
    {-1}, {-1}, {-1}, {-1},
#line 120 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str575, ei_iso8859_8},
    {-1}, {-1},
#line 252 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str578, ei_tis620},
#line 20 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str579, ei_ascii},
#line 193 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str580, ei_cp1256},
    {-1}, {-1}, {-1}, {-1},
#line 317 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str585, ei_iso2022_jp},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 56 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str591, ei_iso8859_1},
#line 97 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str592, ei_iso8859_6},
    {-1}, {-1}, {-1}, {-1},
#line 66 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str597, ei_iso8859_2},
    {-1}, {-1},
#line 190 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str600, ei_cp1255},
#line 310 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str601, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 221 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str609, ei_mac_ukraine},
    {-1}, {-1}, {-1}, {-1},
#line 334 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str614, ei_gb18030_2022},
    {-1}, {-1}, {-1},
#line 29 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str618, ei_ucs2be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 255 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str627, ei_tis620},
    {-1},
#line 271 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str629, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 333 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str636, ei_gb18030_2005},
    {-1}, {-1},
#line 195 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str639, ei_cp1257},
    {-1},
#line 41 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str641, ei_utf16le},
    {-1}, {-1},
#line 216 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str644, ei_mac_centraleurope},
#line 363 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str645, ei_johab},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 222 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str653, ei_mac_greek},
    {-1}, {-1},
#line 40 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str656, ei_utf16be},
    {-1}, {-1}, {-1}, {-1},
#line 184 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str661, ei_cp1253},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 282 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str667, ei_jisx0212},
#line 45 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str668, ei_utf7},
    {-1}, {-1}, {-1}, {-1},
#line 309 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str673, ei_sjis},
#line 107 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str674, ei_iso8859_7},
    {-1},
#line 307 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str676, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 270 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str687, ei_jisx0201},
    {-1}, {-1},
#line 355 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str690, ei_big5hkscs2008},
    {-1},
#line 257 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str692, ei_cp874},
    {-1}, {-1},
#line 274 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str695, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 354 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str701, ei_big5hkscs2008},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 284 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str710, ei_jisx0212},
    {-1},
#line 313 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str712, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 264 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str719, ei_tcvn},
    {-1}, {-1}, {-1},
#line 347 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str723, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 44 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str729, ei_utf32le},
#line 119 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str730, ei_iso8859_8},
    {-1}, {-1}, {-1},
#line 346 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str734, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 276 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str740, ei_jisx0208},
    {-1}, {-1}, {-1},
#line 43 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str744, ei_utf32be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 223 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str751, ei_mac_turkish},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 106 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str759, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 231 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str772, ei_nextstep},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 281 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str785, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 275 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str791, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 49 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str797, ei_ucs2swapped},
#line 202 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str798, ei_cp850},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 18 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str815, ei_ascii},
    {-1}, {-1},
#line 17 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str818, ei_ascii},
    {-1},
#line 51 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str820, ei_ucs4swapped},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 196 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str871, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 273 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str879, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 283 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str914, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 352 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str929, ei_big5hkscs2001},
    {-1}, {-1}, {-1},
#line 356 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str933, ei_big5hkscs2008},
#line 351 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str934, ei_big5hkscs1999},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 308 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str944, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 224 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str956, ei_mac_hebrew},
    {-1},
#line 353 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str958, ei_big5hkscs2004},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 365 "lib/aliases_syssolaris.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1014, ei_johab}
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
