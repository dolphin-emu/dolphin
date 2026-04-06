/* ANSI-C code produced by gperf version 3.2 */
/* Command-line: gperf -m 10 lib/aliases_sysaix.gperf  */
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

#line 1 "lib/aliases_sysaix.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 360
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 5
#define MAX_HASH_VALUE 1039
/* maximum key range = 1035, duplicates = 0 */

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
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040,    0,  174, 1040,   44,    2,
        16,   28,    9,   13,    5,  188,   20,    0,  170, 1040,
      1040, 1040, 1040, 1040, 1040,    9,  191,    2,   19,   65,
       140,   65,   42,   88,  376,  199,    6,  206,    3,    0,
        52, 1040,    0,   11,  118,   97,   59,  189,  220,   13,
         4, 1040, 1040, 1040, 1040,    1, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040,
      1040, 1040, 1040, 1040, 1040, 1040, 1040, 1040
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
    char stringpool_str5[sizeof("C99")];
    char stringpool_str7[sizeof("CN")];
    char stringpool_str10[sizeof("L1")];
    char stringpool_str13[sizeof("L6")];
    char stringpool_str15[sizeof("CHAR")];
    char stringpool_str16[sizeof("CP949")];
    char stringpool_str17[sizeof("L4")];
    char stringpool_str21[sizeof("L5")];
    char stringpool_str22[sizeof("R8")];
    char stringpool_str24[sizeof("L2")];
    char stringpool_str28[sizeof("L8")];
    char stringpool_str29[sizeof("CP819")];
    char stringpool_str33[sizeof("866")];
    char stringpool_str36[sizeof("L3")];
    char stringpool_str40[sizeof("CP154")];
    char stringpool_str42[sizeof("CP866")];
    char stringpool_str43[sizeof("CP1251")];
    char stringpool_str44[sizeof("CP1131")];
    char stringpool_str45[sizeof("CP936")];
    char stringpool_str47[sizeof("CP1361")];
    char stringpool_str48[sizeof("HZ")];
    char stringpool_str49[sizeof("CP1256")];
    char stringpool_str55[sizeof("862")];
    char stringpool_str57[sizeof("CP1254")];
    char stringpool_str64[sizeof("CP862")];
    char stringpool_str65[sizeof("CP1255")];
    char stringpool_str67[sizeof("CP932")];
    char stringpool_str71[sizeof("CP1252")];
    char stringpool_str79[sizeof("CP1258")];
    char stringpool_str80[sizeof("EUCCN")];
    char stringpool_str81[sizeof("EUC-CN")];
    char stringpool_str90[sizeof("PT154")];
    char stringpool_str95[sizeof("CP1253")];
    char stringpool_str96[sizeof("CP1133")];
    char stringpool_str97[sizeof("L10")];
    char stringpool_str101[sizeof("RK1048")];
    char stringpool_str102[sizeof("CP50221")];
    char stringpool_str104[sizeof("UHC")];
    char stringpool_str108[sizeof("CP950")];
    char stringpool_str110[sizeof("US")];
    char stringpool_str111[sizeof("850")];
    char stringpool_str124[sizeof("ISO646-CN")];
    char stringpool_str125[sizeof("SJIS")];
    char stringpool_str127[sizeof("CP1250")];
    char stringpool_str128[sizeof("CP850")];
    char stringpool_str131[sizeof("UCS-4")];
    char stringpool_str136[sizeof("CSUCS4")];
    char stringpool_str145[sizeof("UCS-2")];
    char stringpool_str146[sizeof("PTCP154")];
    char stringpool_str149[sizeof("GB2312")];
    char stringpool_str150[sizeof("ISO8859-9")];
    char stringpool_str151[sizeof("ISO-8859-9")];
    char stringpool_str152[sizeof("ISO_8859-9")];
    char stringpool_str154[sizeof("ISO8859-1")];
    char stringpool_str155[sizeof("ISO-8859-1")];
    char stringpool_str156[sizeof("ISO_8859-1")];
    char stringpool_str157[sizeof("ISO8859-11")];
    char stringpool_str158[sizeof("ISO-8859-11")];
    char stringpool_str159[sizeof("ISO_8859-11")];
    char stringpool_str160[sizeof("ISO8859-6")];
    char stringpool_str161[sizeof("ISO-8859-6")];
    char stringpool_str162[sizeof("ISO_8859-6")];
    char stringpool_str163[sizeof("ISO8859-16")];
    char stringpool_str164[sizeof("ISO-8859-16")];
    char stringpool_str165[sizeof("ISO_8859-16")];
    char stringpool_str167[sizeof("ISO_8859-16:2001")];
    char stringpool_str168[sizeof("ISO8859-4")];
    char stringpool_str169[sizeof("ISO-8859-4")];
    char stringpool_str170[sizeof("ISO_8859-4")];
    char stringpool_str171[sizeof("ISO8859-14")];
    char stringpool_str172[sizeof("ISO-8859-14")];
    char stringpool_str173[sizeof("ISO_8859-14")];
    char stringpool_str176[sizeof("ISO8859-5")];
    char stringpool_str177[sizeof("ISO-8859-5")];
    char stringpool_str178[sizeof("ISO_8859-5")];
    char stringpool_str179[sizeof("ISO8859-15")];
    char stringpool_str180[sizeof("ISO-8859-15")];
    char stringpool_str181[sizeof("ISO_8859-15")];
    char stringpool_str182[sizeof("ISO8859-2")];
    char stringpool_str183[sizeof("ISO-8859-2")];
    char stringpool_str184[sizeof("ISO_8859-2")];
    char stringpool_str187[sizeof("TCVN")];
    char stringpool_str188[sizeof("ISO-IR-199")];
    char stringpool_str189[sizeof("ISO_8859-14:1998")];
    char stringpool_str190[sizeof("ISO8859-8")];
    char stringpool_str191[sizeof("ISO-8859-8")];
    char stringpool_str192[sizeof("ISO_8859-8")];
    char stringpool_str193[sizeof("ISO_8859-15:1998")];
    char stringpool_str194[sizeof("ISO-IR-6")];
    char stringpool_str196[sizeof("L7")];
    char stringpool_str197[sizeof("ISO-IR-149")];
    char stringpool_str199[sizeof("ISO-2022-CN")];
    char stringpool_str201[sizeof("ISO-IR-159")];
    char stringpool_str202[sizeof("CYRILLIC")];
    char stringpool_str203[sizeof("ISO-IR-166")];
    char stringpool_str205[sizeof("ISO-IR-14")];
    char stringpool_str206[sizeof("ISO8859-3")];
    char stringpool_str207[sizeof("ISO-8859-3")];
    char stringpool_str208[sizeof("ISO_8859-3")];
    char stringpool_str209[sizeof("ISO8859-13")];
    char stringpool_str210[sizeof("ISO-8859-13")];
    char stringpool_str211[sizeof("ISO_8859-13")];
    char stringpool_str212[sizeof("CSISO2022CN")];
    char stringpool_str213[sizeof("MAC")];
    char stringpool_str214[sizeof("ISO-IR-126")];
    char stringpool_str215[sizeof("ISO-IR-144")];
    char stringpool_str219[sizeof("ISO-IR-165")];
    char stringpool_str220[sizeof("ISO_8859-10:1992")];
    char stringpool_str222[sizeof("LATIN-9")];
    char stringpool_str225[sizeof("LATIN1")];
    char stringpool_str226[sizeof("GB_1988-80")];
    char stringpool_str228[sizeof("ISO-IR-226")];
    char stringpool_str229[sizeof("CYRILLIC-ASIAN")];
    char stringpool_str231[sizeof("LATIN6")];
    char stringpool_str232[sizeof("ISO-IR-109")];
    char stringpool_str233[sizeof("CP874")];
    char stringpool_str235[sizeof("ISO646-US")];
    char stringpool_str236[sizeof("ISO-IR-101")];
    char stringpool_str237[sizeof("ISO-IR-148")];
    char stringpool_str238[sizeof("ISO-IR-58")];
    char stringpool_str239[sizeof("LATIN4")];
    char stringpool_str240[sizeof("MS-CYRL")];
    char stringpool_str241[sizeof("ISO8859-10")];
    char stringpool_str242[sizeof("ISO-8859-10")];
    char stringpool_str243[sizeof("ISO_8859-10")];
    char stringpool_str244[sizeof("TIS620")];
    char stringpool_str245[sizeof("TIS-620")];
    char stringpool_str246[sizeof("GB_2312-80")];
    char stringpool_str247[sizeof("LATIN5")];
    char stringpool_str248[sizeof("ELOT_928")];
    char stringpool_str249[sizeof("MS936")];
    char stringpool_str253[sizeof("LATIN2")];
    char stringpool_str254[sizeof("GB18030")];
    char stringpool_str255[sizeof("UTF-16")];
    char stringpool_str256[sizeof("ISO-IR-138")];
    char stringpool_str260[sizeof("UCS-4LE")];
    char stringpool_str261[sizeof("LATIN8")];
    char stringpool_str264[sizeof("ROMAN8")];
    char stringpool_str267[sizeof("UCS-2LE")];
    char stringpool_str268[sizeof("CSPTCP154")];
    char stringpool_str270[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str271[sizeof("EUCKR")];
    char stringpool_str272[sizeof("EUC-KR")];
    char stringpool_str274[sizeof("ISO-10646-UCS-4")];
    char stringpool_str275[sizeof("X0212")];
    char stringpool_str276[sizeof("KSC_5601")];
    char stringpool_str277[sizeof("LATIN3")];
    char stringpool_str278[sizeof("ISO-IR-110")];
    char stringpool_str280[sizeof("ASCII")];
    char stringpool_str281[sizeof("ISO-10646-UCS-2")];
    char stringpool_str282[sizeof("UTF-8")];
    char stringpool_str283[sizeof("KS_C_5601-1989")];
    char stringpool_str285[sizeof("KOREAN")];
    char stringpool_str286[sizeof("BIG5")];
    char stringpool_str287[sizeof("BIG-5")];
    char stringpool_str288[sizeof("UNICODE-1-1")];
    char stringpool_str289[sizeof("X0201")];
    char stringpool_str290[sizeof("TIS620-0")];
    char stringpool_str293[sizeof("CSUNICODE11")];
    char stringpool_str295[sizeof("CSASCII")];
    char stringpool_str301[sizeof("KZ-1048")];
    char stringpool_str302[sizeof("ISO-IR-203")];
    char stringpool_str303[sizeof("UTF-32")];
    char stringpool_str305[sizeof("MULELAO-1")];
    char stringpool_str306[sizeof("CHINESE")];
    char stringpool_str307[sizeof("ARABIC")];
    char stringpool_str308[sizeof("CSKZ1048")];
    char stringpool_str309[sizeof("HP-ROMAN8")];
    char stringpool_str310[sizeof("ECMA-114")];
    char stringpool_str312[sizeof("LATIN10")];
    char stringpool_str313[sizeof("KOI8-R")];
    char stringpool_str315[sizeof("GEORGIAN-PS")];
    char stringpool_str316[sizeof("CSKOI8R")];
    char stringpool_str318[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str320[sizeof("ISO-IR-100")];
    char stringpool_str321[sizeof("IBM-921")];
    char stringpool_str322[sizeof("IBM819")];
    char stringpool_str325[sizeof("X0208")];
    char stringpool_str327[sizeof("ISO_8859-9:1989")];
    char stringpool_str332[sizeof("ECMA-118")];
    char stringpool_str335[sizeof("IBM866")];
    char stringpool_str338[sizeof("IBM-1131")];
    char stringpool_str340[sizeof("CSISOLATIN1")];
    char stringpool_str342[sizeof("VISCII")];
    char stringpool_str344[sizeof("CSGB2312")];
    char stringpool_str346[sizeof("CSISOLATIN6")];
    char stringpool_str347[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str350[sizeof("CSUNICODE")];
    char stringpool_str352[sizeof("CSISOLATINARABIC")];
    char stringpool_str354[sizeof("CSISOLATIN4")];
    char stringpool_str356[sizeof("ISO_8859-4:1988")];
    char stringpool_str357[sizeof("IBM862")];
    char stringpool_str360[sizeof("ISO_8859-5:1988")];
    char stringpool_str361[sizeof("IBM-932")];
    char stringpool_str362[sizeof("CSISOLATIN5")];
    char stringpool_str364[sizeof("CSHPROMAN8")];
    char stringpool_str365[sizeof("IBM-1252")];
    char stringpool_str367[sizeof("ISO_8859-8:1988")];
    char stringpool_str368[sizeof("CSISOLATIN2")];
    char stringpool_str372[sizeof("CSEUCKR")];
    char stringpool_str374[sizeof("STRK1048-2002")];
    char stringpool_str375[sizeof("ISO_8859-3:1988")];
    char stringpool_str376[sizeof("ISO-IR-179")];
    char stringpool_str378[sizeof("CSBIG5")];
    char stringpool_str379[sizeof("CN-BIG5")];
    char stringpool_str381[sizeof("ISO-CELTIC")];
    char stringpool_str386[sizeof("HZ-GB-2312")];
    char stringpool_str388[sizeof("UTF-16LE")];
    char stringpool_str390[sizeof("ISO-2022-KR")];
    char stringpool_str391[sizeof("US-ASCII")];
    char stringpool_str392[sizeof("CSISOLATIN3")];
    char stringpool_str403[sizeof("CSISO2022KR")];
    char stringpool_str406[sizeof("MS-EE")];
    char stringpool_str411[sizeof("UCS-4-INTERNAL")];
    char stringpool_str412[sizeof("MS-ANSI")];
    char stringpool_str413[sizeof("TCVN5712-1")];
    char stringpool_str415[sizeof("CP1257")];
    char stringpool_str416[sizeof("CP367")];
    char stringpool_str418[sizeof("UCS-2-INTERNAL")];
    char stringpool_str421[sizeof("IBM850")];
    char stringpool_str422[sizeof("IBM-850")];
    char stringpool_str424[sizeof("TCVN-5712")];
    char stringpool_str425[sizeof("UTF-32LE")];
    char stringpool_str426[sizeof("MACCYRILLIC")];
    char stringpool_str428[sizeof("TIS620.2529-1")];
    char stringpool_str429[sizeof("MACICELAND")];
    char stringpool_str430[sizeof("JP")];
    char stringpool_str434[sizeof("CSVISCII")];
    char stringpool_str437[sizeof("MACROMAN")];
    char stringpool_str440[sizeof("GREEK8")];
    char stringpool_str441[sizeof("VISCII1.1-1")];
    char stringpool_str445[sizeof("UCS-4BE")];
    char stringpool_str446[sizeof("IBM-CP1133")];
    char stringpool_str447[sizeof("ISO_646.IRV:1991")];
    char stringpool_str451[sizeof("MACCROATIAN")];
    char stringpool_str452[sizeof("UCS-2BE")];
    char stringpool_str453[sizeof("ARMSCII-8")];
    char stringpool_str454[sizeof("CN-GB")];
    char stringpool_str456[sizeof("WINDOWS-1251")];
    char stringpool_str457[sizeof("JAVA")];
    char stringpool_str459[sizeof("WINDOWS-1256")];
    char stringpool_str460[sizeof("WINDOWS-936")];
    char stringpool_str462[sizeof("UCS-4-SWAPPED")];
    char stringpool_str463[sizeof("WINDOWS-1254")];
    char stringpool_str466[sizeof("GBK")];
    char stringpool_str467[sizeof("WINDOWS-1255")];
    char stringpool_str468[sizeof("TIS620.2533-1")];
    char stringpool_str469[sizeof("UCS-2-SWAPPED")];
    char stringpool_str470[sizeof("WINDOWS-1252")];
    char stringpool_str471[sizeof("KS_C_5601-1987")];
    char stringpool_str472[sizeof("CN-GB-ISOIR165")];
    char stringpool_str474[sizeof("WINDOWS-1258")];
    char stringpool_str475[sizeof("IBM-EUCCN")];
    char stringpool_str476[sizeof("JIS_C6226-1983")];
    char stringpool_str477[sizeof("GB18030:2022")];
    char stringpool_str480[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str481[sizeof("CSKSC56011987")];
    char stringpool_str482[sizeof("WINDOWS-1253")];
    char stringpool_str483[sizeof("CSUNICODE11UTF7")];
    char stringpool_str484[sizeof("WCHAR_T")];
    char stringpool_str490[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str493[sizeof("CSISO58GB231280")];
    char stringpool_str495[sizeof("ASMO-708")];
    char stringpool_str498[sizeof("WINDOWS-1250")];
    char stringpool_str502[sizeof("GB18030:2005")];
    char stringpool_str507[sizeof("KOI8-U")];
    char stringpool_str508[sizeof("KOI8-RU")];
    char stringpool_str510[sizeof("TIS620.2533-0")];
    char stringpool_str511[sizeof("MS-HEBR")];
    char stringpool_str517[sizeof("ISO_8859-1:1987")];
    char stringpool_str518[sizeof("MACARABIC")];
    char stringpool_str520[sizeof("ISO_8859-6:1987")];
    char stringpool_str521[sizeof("MACINTOSH")];
    char stringpool_str525[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str526[sizeof("ISO8859-7")];
    char stringpool_str527[sizeof("ISO-8859-7")];
    char stringpool_str528[sizeof("ISO_8859-7")];
    char stringpool_str530[sizeof("CSIBM866")];
    char stringpool_str531[sizeof("ISO_8859-2:1987")];
    char stringpool_str534[sizeof("CSMACINTOSH")];
    char stringpool_str538[sizeof("JIS0208")];
    char stringpool_str542[sizeof("MACROMANIA")];
    char stringpool_str543[sizeof("ISO_8859-7:2003")];
    char stringpool_str549[sizeof("KOI8-T")];
    char stringpool_str552[sizeof("EUCJP")];
    char stringpool_str553[sizeof("EUC-JP")];
    char stringpool_str554[sizeof("BIG5HKSCS")];
    char stringpool_str555[sizeof("BIG5-HKSCS")];
    char stringpool_str557[sizeof("MACCENTRALEUROPE")];
    char stringpool_str560[sizeof("MACTHAI")];
    char stringpool_str561[sizeof("ANSI_X3.4-1986")];
    char stringpool_str565[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str568[sizeof("EUCTW")];
    char stringpool_str569[sizeof("EUC-TW")];
    char stringpool_str572[sizeof("CSISOLATINHEBREW")];
    char stringpool_str573[sizeof("UTF-16BE")];
    char stringpool_str574[sizeof("ISO-IR-57")];
    char stringpool_str576[sizeof("ANSI_X3.4-1968")];
    char stringpool_str577[sizeof("ISO-IR-157")];
    char stringpool_str580[sizeof("ISO-IR-127")];
    char stringpool_str581[sizeof("ISO-IR-87")];
    char stringpool_str585[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str593[sizeof("CSISO57GB1988")];
    char stringpool_str596[sizeof("ISO646-JP")];
    char stringpool_str597[sizeof("LATIN7")];
    char stringpool_str598[sizeof("GREEK")];
    char stringpool_str604[sizeof("CSISOLATINGREEK")];
    char stringpool_str606[sizeof("CSISO14JISC6220RO")];
    char stringpool_str610[sizeof("UTF-32BE")];
    char stringpool_str613[sizeof("MS-ARAB")];
    char stringpool_str614[sizeof("TCVN5712-1:1993")];
    char stringpool_str618[sizeof("UTF-7")];
    char stringpool_str623[sizeof("ISO-2022-JP-1")];
    char stringpool_str633[sizeof("ISO-2022-JP-MS")];
    char stringpool_str637[sizeof("ISO-2022-JP-2")];
    char stringpool_str642[sizeof("WINDOWS-1257")];
    char stringpool_str647[sizeof("NEXTSTEP")];
    char stringpool_str648[sizeof("WINDOWS-874")];
    char stringpool_str649[sizeof("CSISO2022JP2")];
    char stringpool_str655[sizeof("CSISO159JISX02121990")];
    char stringpool_str666[sizeof("IBM-EUCKR")];
    char stringpool_str669[sizeof("CSEUCTW")];
    char stringpool_str671[sizeof("ISO-2022-JP")];
    char stringpool_str679[sizeof("UNICODELITTLE")];
    char stringpool_str680[sizeof("BIGFIVE")];
    char stringpool_str681[sizeof("BIG-FIVE")];
    char stringpool_str682[sizeof("HEBREW")];
    char stringpool_str683[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str684[sizeof("CSISO2022JP")];
    char stringpool_str690[sizeof("UNICODEBIG")];
    char stringpool_str703[sizeof("ISO_8859-7:1987")];
    char stringpool_str709[sizeof("IBM367")];
    char stringpool_str711[sizeof("JIS_X0212")];
    char stringpool_str719[sizeof("BIG5-HKSCS:1999")];
    char stringpool_str721[sizeof("BIG5-HKSCS:2001")];
    char stringpool_str725[sizeof("JIS_X0201")];
    char stringpool_str728[sizeof("BIG5-HKSCS:2004")];
    char stringpool_str733[sizeof("JISX0201-1976")];
    char stringpool_str739[sizeof("BIG5-HKSCS:2008")];
    char stringpool_str744[sizeof("MACUKRAINE")];
    char stringpool_str746[sizeof("JIS_X0212-1990")];
    char stringpool_str761[sizeof("JIS_X0208")];
    char stringpool_str776[sizeof("JIS_X0208-1983")];
    char stringpool_str792[sizeof("JIS_X0208-1990")];
    char stringpool_str807[sizeof("MS-GREEK")];
    char stringpool_str809[sizeof("MACGREEK")];
    char stringpool_str814[sizeof("JOHAB")];
    char stringpool_str815[sizeof("MACTURKISH")];
    char stringpool_str826[sizeof("MS-TURK")];
    char stringpool_str852[sizeof("SHIFT-JIS")];
    char stringpool_str853[sizeof("SHIFT_JIS")];
    char stringpool_str897[sizeof("CSSHIFTJIS")];
    char stringpool_str922[sizeof("JIS_X0212.1990-0")];
    char stringpool_str947[sizeof("IBM-EUCJP")];
    char stringpool_str958[sizeof("MACHEBREW")];
    char stringpool_str963[sizeof("IBM-EUCTW")];
    char stringpool_str978[sizeof("MS_KANJI")];
    char stringpool_str984[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str1026[sizeof("WINBALTRIM")];
    char stringpool_str1039[sizeof("CSISO87JISX0208")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "C99",
    "CN",
    "L1",
    "L6",
    "CHAR",
    "CP949",
    "L4",
    "L5",
    "R8",
    "L2",
    "L8",
    "CP819",
    "866",
    "L3",
    "CP154",
    "CP866",
    "CP1251",
    "CP1131",
    "CP936",
    "CP1361",
    "HZ",
    "CP1256",
    "862",
    "CP1254",
    "CP862",
    "CP1255",
    "CP932",
    "CP1252",
    "CP1258",
    "EUCCN",
    "EUC-CN",
    "PT154",
    "CP1253",
    "CP1133",
    "L10",
    "RK1048",
    "CP50221",
    "UHC",
    "CP950",
    "US",
    "850",
    "ISO646-CN",
    "SJIS",
    "CP1250",
    "CP850",
    "UCS-4",
    "CSUCS4",
    "UCS-2",
    "PTCP154",
    "GB2312",
    "ISO8859-9",
    "ISO-8859-9",
    "ISO_8859-9",
    "ISO8859-1",
    "ISO-8859-1",
    "ISO_8859-1",
    "ISO8859-11",
    "ISO-8859-11",
    "ISO_8859-11",
    "ISO8859-6",
    "ISO-8859-6",
    "ISO_8859-6",
    "ISO8859-16",
    "ISO-8859-16",
    "ISO_8859-16",
    "ISO_8859-16:2001",
    "ISO8859-4",
    "ISO-8859-4",
    "ISO_8859-4",
    "ISO8859-14",
    "ISO-8859-14",
    "ISO_8859-14",
    "ISO8859-5",
    "ISO-8859-5",
    "ISO_8859-5",
    "ISO8859-15",
    "ISO-8859-15",
    "ISO_8859-15",
    "ISO8859-2",
    "ISO-8859-2",
    "ISO_8859-2",
    "TCVN",
    "ISO-IR-199",
    "ISO_8859-14:1998",
    "ISO8859-8",
    "ISO-8859-8",
    "ISO_8859-8",
    "ISO_8859-15:1998",
    "ISO-IR-6",
    "L7",
    "ISO-IR-149",
    "ISO-2022-CN",
    "ISO-IR-159",
    "CYRILLIC",
    "ISO-IR-166",
    "ISO-IR-14",
    "ISO8859-3",
    "ISO-8859-3",
    "ISO_8859-3",
    "ISO8859-13",
    "ISO-8859-13",
    "ISO_8859-13",
    "CSISO2022CN",
    "MAC",
    "ISO-IR-126",
    "ISO-IR-144",
    "ISO-IR-165",
    "ISO_8859-10:1992",
    "LATIN-9",
    "LATIN1",
    "GB_1988-80",
    "ISO-IR-226",
    "CYRILLIC-ASIAN",
    "LATIN6",
    "ISO-IR-109",
    "CP874",
    "ISO646-US",
    "ISO-IR-101",
    "ISO-IR-148",
    "ISO-IR-58",
    "LATIN4",
    "MS-CYRL",
    "ISO8859-10",
    "ISO-8859-10",
    "ISO_8859-10",
    "TIS620",
    "TIS-620",
    "GB_2312-80",
    "LATIN5",
    "ELOT_928",
    "MS936",
    "LATIN2",
    "GB18030",
    "UTF-16",
    "ISO-IR-138",
    "UCS-4LE",
    "LATIN8",
    "ROMAN8",
    "UCS-2LE",
    "CSPTCP154",
    "GEORGIAN-ACADEMY",
    "EUCKR",
    "EUC-KR",
    "ISO-10646-UCS-4",
    "X0212",
    "KSC_5601",
    "LATIN3",
    "ISO-IR-110",
    "ASCII",
    "ISO-10646-UCS-2",
    "UTF-8",
    "KS_C_5601-1989",
    "KOREAN",
    "BIG5",
    "BIG-5",
    "UNICODE-1-1",
    "X0201",
    "TIS620-0",
    "CSUNICODE11",
    "CSASCII",
    "KZ-1048",
    "ISO-IR-203",
    "UTF-32",
    "MULELAO-1",
    "CHINESE",
    "ARABIC",
    "CSKZ1048",
    "HP-ROMAN8",
    "ECMA-114",
    "LATIN10",
    "KOI8-R",
    "GEORGIAN-PS",
    "CSKOI8R",
    "ISO-2022-CN-EXT",
    "ISO-IR-100",
    "IBM-921",
    "IBM819",
    "X0208",
    "ISO_8859-9:1989",
    "ECMA-118",
    "IBM866",
    "IBM-1131",
    "CSISOLATIN1",
    "VISCII",
    "CSGB2312",
    "CSISOLATIN6",
    "CSISOLATINCYRILLIC",
    "CSUNICODE",
    "CSISOLATINARABIC",
    "CSISOLATIN4",
    "ISO_8859-4:1988",
    "IBM862",
    "ISO_8859-5:1988",
    "IBM-932",
    "CSISOLATIN5",
    "CSHPROMAN8",
    "IBM-1252",
    "ISO_8859-8:1988",
    "CSISOLATIN2",
    "CSEUCKR",
    "STRK1048-2002",
    "ISO_8859-3:1988",
    "ISO-IR-179",
    "CSBIG5",
    "CN-BIG5",
    "ISO-CELTIC",
    "HZ-GB-2312",
    "UTF-16LE",
    "ISO-2022-KR",
    "US-ASCII",
    "CSISOLATIN3",
    "CSISO2022KR",
    "MS-EE",
    "UCS-4-INTERNAL",
    "MS-ANSI",
    "TCVN5712-1",
    "CP1257",
    "CP367",
    "UCS-2-INTERNAL",
    "IBM850",
    "IBM-850",
    "TCVN-5712",
    "UTF-32LE",
    "MACCYRILLIC",
    "TIS620.2529-1",
    "MACICELAND",
    "JP",
    "CSVISCII",
    "MACROMAN",
    "GREEK8",
    "VISCII1.1-1",
    "UCS-4BE",
    "IBM-CP1133",
    "ISO_646.IRV:1991",
    "MACCROATIAN",
    "UCS-2BE",
    "ARMSCII-8",
    "CN-GB",
    "WINDOWS-1251",
    "JAVA",
    "WINDOWS-1256",
    "WINDOWS-936",
    "UCS-4-SWAPPED",
    "WINDOWS-1254",
    "GBK",
    "WINDOWS-1255",
    "TIS620.2533-1",
    "UCS-2-SWAPPED",
    "WINDOWS-1252",
    "KS_C_5601-1987",
    "CN-GB-ISOIR165",
    "WINDOWS-1258",
    "IBM-EUCCN",
    "JIS_C6226-1983",
    "GB18030:2022",
    "UNICODE-1-1-UTF-7",
    "CSKSC56011987",
    "WINDOWS-1253",
    "CSUNICODE11UTF7",
    "WCHAR_T",
    "JIS_C6220-1969-RO",
    "CSISO58GB231280",
    "ASMO-708",
    "WINDOWS-1250",
    "GB18030:2005",
    "KOI8-U",
    "KOI8-RU",
    "TIS620.2533-0",
    "MS-HEBR",
    "ISO_8859-1:1987",
    "MACARABIC",
    "ISO_8859-6:1987",
    "MACINTOSH",
    "CSPC862LATINHEBREW",
    "ISO8859-7",
    "ISO-8859-7",
    "ISO_8859-7",
    "CSIBM866",
    "ISO_8859-2:1987",
    "CSMACINTOSH",
    "JIS0208",
    "MACROMANIA",
    "ISO_8859-7:2003",
    "KOI8-T",
    "EUCJP",
    "EUC-JP",
    "BIG5HKSCS",
    "BIG5-HKSCS",
    "MACCENTRALEUROPE",
    "MACTHAI",
    "ANSI_X3.4-1986",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "EUCTW",
    "EUC-TW",
    "CSISOLATINHEBREW",
    "UTF-16BE",
    "ISO-IR-57",
    "ANSI_X3.4-1968",
    "ISO-IR-157",
    "ISO-IR-127",
    "ISO-IR-87",
    "CSPC850MULTILINGUAL",
    "CSISO57GB1988",
    "ISO646-JP",
    "LATIN7",
    "GREEK",
    "CSISOLATINGREEK",
    "CSISO14JISC6220RO",
    "UTF-32BE",
    "MS-ARAB",
    "TCVN5712-1:1993",
    "UTF-7",
    "ISO-2022-JP-1",
    "ISO-2022-JP-MS",
    "ISO-2022-JP-2",
    "WINDOWS-1257",
    "NEXTSTEP",
    "WINDOWS-874",
    "CSISO2022JP2",
    "CSISO159JISX02121990",
    "IBM-EUCKR",
    "CSEUCTW",
    "ISO-2022-JP",
    "UNICODELITTLE",
    "BIGFIVE",
    "BIG-FIVE",
    "HEBREW",
    "CSHALFWIDTHKATAKANA",
    "CSISO2022JP",
    "UNICODEBIG",
    "ISO_8859-7:1987",
    "IBM367",
    "JIS_X0212",
    "BIG5-HKSCS:1999",
    "BIG5-HKSCS:2001",
    "JIS_X0201",
    "BIG5-HKSCS:2004",
    "JISX0201-1976",
    "BIG5-HKSCS:2008",
    "MACUKRAINE",
    "JIS_X0212-1990",
    "JIS_X0208",
    "JIS_X0208-1983",
    "JIS_X0208-1990",
    "MS-GREEK",
    "MACGREEK",
    "JOHAB",
    "MACTURKISH",
    "MS-TURK",
    "SHIFT-JIS",
    "SHIFT_JIS",
    "CSSHIFTJIS",
    "JIS_X0212.1990-0",
    "IBM-EUCJP",
    "MACHEBREW",
    "IBM-EUCTW",
    "MS_KANJI",
    "CSEUCPKDFMTJAPANESE",
    "WINBALTRIM",
    "CSISO87JISX0208"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 51 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str5, ei_c99},
    {-1},
#line 292 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str7, ei_iso646_cn},
    {-1}, {-1},
#line 60 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str10, ei_iso8859_1},
    {-1}, {-1},
#line 134 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str13, ei_iso8859_10},
    {-1},
#line 370 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15, ei_local_char},
#line 364 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str16, ei_cp949},
#line 84 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17, ei_iso8859_4},
    {-1}, {-1}, {-1},
#line 126 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str21, ei_iso8859_9},
#line 231 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22, ei_hp_roman8},
    {-1},
#line 68 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str24, ei_iso8859_2},
    {-1}, {-1}, {-1},
#line 152 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str28, ei_iso8859_14},
#line 57 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29, ei_iso8859_1},
    {-1}, {-1}, {-1},
#line 210 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str33, ei_cp866},
    {-1}, {-1},
#line 76 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str36, ei_iso8859_3},
    {-1}, {-1}, {-1},
#line 240 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str40, ei_pt154},
    {-1},
#line 208 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str42, ei_cp866},
#line 175 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str43, ei_cp1251},
#line 212 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str44, ei_cp1131},
#line 332 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str45, ei_cp936},
    {-1},
#line 367 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str47, ei_johab},
#line 341 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str48, ei_hz},
#line 191 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str49, ei_cp1256},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 206 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str55, ei_cp862},
    {-1},
#line 185 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str57, ei_cp1254},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 204 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str64, ei_cp862},
#line 188 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str65, ei_cp1255},
    {-1},
#line 316 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str67, ei_cp932},
    {-1}, {-1}, {-1},
#line 178 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str71, ei_cp1252},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 197 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str79, ei_cp1258},
#line 326 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str80, ei_euc_cn},
#line 325 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str81, ei_euc_cn},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 238 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str90, ei_pt154},
    {-1}, {-1}, {-1}, {-1},
#line 182 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str95, ei_cp1253},
#line 248 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str96, ei_cp1133},
#line 166 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str97, ei_iso8859_16},
    {-1}, {-1}, {-1},
#line 243 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str101, ei_rk1048},
#line 324 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str102, ei_iso2022_jpms},
    {-1},
#line 365 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str104, ei_cp949},
    {-1}, {-1}, {-1},
#line 353 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str108, ei_cp950},
    {-1},
#line 21 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str110, ei_ascii},
#line 201 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str111, ei_cp850},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 290 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str124, ei_iso646_cn},
#line 313 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str125, ei_sjis},
    {-1},
#line 172 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str127, ei_cp1250},
#line 199 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str128, ei_cp850},
    {-1}, {-1},
#line 33 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str131, ei_ucs4},
    {-1}, {-1}, {-1}, {-1},
#line 35 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str136, ei_ucs4},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 24 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str145, ei_ucs2},
#line 239 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str146, ei_pt154},
    {-1}, {-1},
#line 327 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str149, ei_euc_cn},
#line 128 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str150, ei_iso8859_9},
#line 121 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str151, ei_iso8859_9},
#line 122 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str152, ei_iso8859_9},
    {-1},
#line 62 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str154, ei_iso8859_1},
#line 53 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str155, ei_iso8859_1},
#line 54 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str156, ei_iso8859_1},
#line 139 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str157, ei_iso8859_11},
#line 137 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str158, ei_iso8859_11},
#line 138 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str159, ei_iso8859_11},
#line 102 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str160, ei_iso8859_6},
#line 94 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str161, ei_iso8859_6},
#line 95 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str162, ei_iso8859_6},
#line 167 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str163, ei_iso8859_16},
#line 161 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str164, ei_iso8859_16},
#line 162 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str165, ei_iso8859_16},
    {-1},
#line 163 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str167, ei_iso8859_16},
#line 86 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str168, ei_iso8859_4},
#line 79 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str169, ei_iso8859_4},
#line 80 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str170, ei_iso8859_4},
#line 154 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str171, ei_iso8859_14},
#line 147 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str172, ei_iso8859_14},
#line 148 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str173, ei_iso8859_14},
    {-1}, {-1},
#line 93 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str176, ei_iso8859_5},
#line 87 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str177, ei_iso8859_5},
#line 88 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str178, ei_iso8859_5},
#line 160 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_15},
#line 155 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str180, ei_iso8859_15},
#line 156 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str181, ei_iso8859_15},
#line 70 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str182, ei_iso8859_2},
#line 63 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str183, ei_iso8859_2},
#line 64 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str184, ei_iso8859_2},
    {-1}, {-1},
#line 262 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str187, ei_tcvn},
#line 150 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str188, ei_iso8859_14},
#line 149 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str189, ei_iso8859_14},
#line 120 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str190, ei_iso8859_8},
#line 114 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str191, ei_iso8859_8},
#line 115 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str192, ei_iso8859_8},
#line 157 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str193, ei_iso8859_15},
#line 16 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str194, ei_ascii},
    {-1},
#line 144 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str196, ei_iso8859_13},
#line 303 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str197, ei_ksc5601},
    {-1},
#line 338 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str199, ei_iso2022_cn},
    {-1},
#line 287 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str201, ei_jisx0212},
#line 91 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str202, ei_iso8859_5},
#line 256 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str203, ei_tis620},
    {-1},
#line 268 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str205, ei_iso646_jp},
#line 78 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str206, ei_iso8859_3},
#line 71 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str207, ei_iso8859_3},
#line 72 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str208, ei_iso8859_3},
#line 145 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str209, ei_iso8859_13},
#line 140 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str210, ei_iso8859_13},
#line 141 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str211, ei_iso8859_13},
#line 339 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str212, ei_iso2022_cn},
#line 216 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str213, ei_mac_roman},
#line 107 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str214, ei_iso8859_7},
#line 90 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str215, ei_iso8859_5},
    {-1}, {-1}, {-1},
#line 298 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str219, ei_isoir165},
#line 131 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str220, ei_iso8859_10},
    {-1},
#line 159 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str222, ei_iso8859_15},
    {-1}, {-1},
#line 59 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str225, ei_iso8859_1},
#line 289 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str226, ei_iso646_cn},
    {-1},
#line 164 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str228, ei_iso8859_16},
#line 241 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str229, ei_pt154},
    {-1},
#line 133 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str231, ei_iso8859_10},
#line 74 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str232, ei_iso8859_3},
#line 257 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str233, ei_cp874},
    {-1},
#line 14 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str235, ei_ascii},
#line 66 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str236, ei_iso8859_2},
#line 124 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str237, ei_iso8859_9},
#line 295 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str238, ei_gb2312},
#line 83 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str239, ei_iso8859_4},
#line 177 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str240, ei_cp1251},
#line 136 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str241, ei_iso8859_10},
#line 129 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str242, ei_iso8859_10},
#line 130 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str243, ei_iso8859_10},
#line 251 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str244, ei_tis620},
#line 250 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str245, ei_tis620},
#line 294 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str246, ei_gb2312},
#line 125 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str247, ei_iso8859_9},
#line 109 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str248, ei_iso8859_7},
#line 333 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str249, ei_cp936},
    {-1}, {-1}, {-1},
#line 67 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str253, ei_iso8859_2},
#line 335 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str254, ei_gb18030_2005},
#line 38 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str255, ei_utf16},
#line 117 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str256, ei_iso8859_8},
    {-1}, {-1}, {-1},
#line 37 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str260, ei_ucs4le},
#line 151 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str261, ei_iso8859_14},
    {-1}, {-1},
#line 230 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str264, ei_hp_roman8},
    {-1}, {-1},
#line 31 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str267, ei_ucs2le},
#line 242 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str268, ei_pt154},
    {-1},
#line 235 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str270, ei_georgian_academy},
#line 361 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str271, ei_euc_kr},
#line 360 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str272, ei_euc_kr},
    {-1},
#line 34 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str274, ei_ucs4},
#line 286 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str275, ei_jisx0212},
#line 300 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str276, ei_ksc5601},
#line 75 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str277, ei_iso8859_3},
#line 82 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str278, ei_iso8859_4},
    {-1},
#line 13 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str280, ei_ascii},
#line 25 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str281, ei_ucs2},
#line 23 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str282, ei_utf8},
#line 302 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str283, ei_ksc5601},
    {-1},
#line 305 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str285, ei_ksc5601},
#line 347 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str286, ei_ces_big5},
#line 348 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str287, ei_ces_big5},
#line 29 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str288, ei_ucs2be},
#line 273 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str289, ei_jisx0201},
#line 252 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str290, ei_tis620},
    {-1}, {-1},
#line 30 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str293, ei_ucs2be},
    {-1},
#line 22 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str295, ei_ascii},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 245 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str301, ei_rk1048},
#line 158 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str302, ei_iso8859_15},
#line 41 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str303, ei_utf32},
    {-1},
#line 247 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str305, ei_mulelao},
#line 297 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str306, ei_gb2312},
#line 100 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str307, ei_iso8859_6},
#line 246 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str308, ei_rk1048},
#line 229 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str309, ei_hp_roman8},
#line 98 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str310, ei_iso8859_6},
    {-1},
#line 165 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str312, ei_iso8859_16},
#line 168 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str313, ei_koi8_r},
    {-1},
#line 236 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str315, ei_georgian_ps},
#line 169 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str316, ei_koi8_r},
    {-1},
#line 340 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str318, ei_iso2022_cn_ext},
    {-1},
#line 56 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str320, ei_iso8859_1},
#line 146 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str321, ei_iso8859_13},
#line 58 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str322, ei_iso8859_1},
    {-1}, {-1},
#line 279 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str325, ei_jisx0208},
    {-1},
#line 123 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str327, ei_iso8859_9},
    {-1}, {-1}, {-1}, {-1},
#line 108 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str332, ei_iso8859_7},
    {-1}, {-1},
#line 209 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str335, ei_cp866},
    {-1}, {-1},
#line 213 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str338, ei_cp1131},
    {-1},
#line 61 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str340, ei_iso8859_1},
    {-1},
#line 259 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str342, ei_viscii},
    {-1},
#line 329 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str344, ei_euc_cn},
    {-1},
#line 135 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str346, ei_iso8859_10},
#line 92 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str347, ei_iso8859_5},
    {-1}, {-1},
#line 26 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str350, ei_ucs2},
    {-1},
#line 101 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str352, ei_iso8859_6},
    {-1},
#line 85 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str354, ei_iso8859_4},
    {-1},
#line 81 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str356, ei_iso8859_4},
#line 205 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str357, ei_cp862},
    {-1}, {-1},
#line 89 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str360, ei_iso8859_5},
#line 317 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str361, ei_cp932},
#line 127 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str362, ei_iso8859_9},
    {-1},
#line 232 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str364, ei_hp_roman8},
#line 181 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str365, ei_cp1252},
    {-1},
#line 116 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str367, ei_iso8859_8},
#line 69 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str368, ei_iso8859_2},
    {-1}, {-1}, {-1},
#line 362 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str372, ei_euc_kr},
    {-1},
#line 244 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str374, ei_rk1048},
#line 73 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str375, ei_iso8859_3},
#line 142 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str376, ei_iso8859_13},
    {-1},
#line 352 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str378, ei_ces_big5},
#line 351 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str379, ei_ces_big5},
    {-1},
#line 153 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str381, ei_iso8859_14},
    {-1}, {-1}, {-1}, {-1},
#line 342 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str386, ei_hz},
    {-1},
#line 40 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str388, ei_utf16le},
    {-1},
#line 368 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str390, ei_iso2022_kr},
#line 12 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str391, ei_ascii},
#line 77 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str392, ei_iso8859_3},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 369 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str403, ei_iso2022_kr},
    {-1}, {-1},
#line 174 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str406, ei_cp1250},
    {-1}, {-1}, {-1}, {-1},
#line 49 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str411, ei_ucs4internal},
#line 180 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str412, ei_cp1252},
#line 264 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str413, ei_tcvn},
    {-1},
#line 194 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str415, ei_cp1257},
#line 19 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str416, ei_ascii},
    {-1},
#line 47 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str418, ei_ucs2internal},
    {-1}, {-1},
#line 200 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str421, ei_cp850},
#line 203 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str422, ei_cp850},
    {-1},
#line 263 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str424, ei_tcvn},
#line 43 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str425, ei_utf32le},
#line 222 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str426, ei_mac_cyrillic},
    {-1},
#line 253 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str428, ei_tis620},
#line 219 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str429, ei_mac_iceland},
#line 269 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str430, ei_iso646_jp},
    {-1}, {-1}, {-1},
#line 261 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str434, ei_viscii},
    {-1}, {-1},
#line 214 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str437, ei_mac_roman},
    {-1}, {-1},
#line 110 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str440, ei_iso8859_7},
#line 260 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str441, ei_viscii},
    {-1}, {-1}, {-1},
#line 36 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str445, ei_ucs4be},
#line 249 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str446, ei_cp1133},
#line 15 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str447, ei_ascii},
    {-1}, {-1}, {-1},
#line 220 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str451, ei_mac_croatian},
#line 27 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str452, ei_ucs2be},
#line 234 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str453, ei_armscii_8},
#line 328 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str454, ei_euc_cn},
    {-1},
#line 176 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str456, ei_cp1251},
#line 52 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str457, ei_java},
    {-1},
#line 192 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str459, ei_cp1256},
#line 334 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str460, ei_cp936},
    {-1},
#line 50 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str462, ei_ucs4swapped},
#line 186 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str463, ei_cp1254},
    {-1}, {-1},
#line 331 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str466, ei_ces_gbk},
#line 189 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str467, ei_cp1255},
#line 255 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str468, ei_tis620},
#line 48 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str469, ei_ucs2swapped},
#line 179 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str470, ei_cp1252},
#line 301 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str471, ei_ksc5601},
#line 299 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str472, ei_isoir165},
    {-1},
#line 198 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str474, ei_cp1258},
#line 330 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str475, ei_euc_cn},
#line 281 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str476, ei_jisx0208},
#line 337 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str477, ei_gb18030_2022},
    {-1}, {-1},
#line 45 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str480, ei_utf7},
#line 304 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str481, ei_ksc5601},
#line 183 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str482, ei_cp1253},
#line 46 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str483, ei_utf7},
#line 371 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str484, ei_local_wchar_t},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 266 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str490, ei_iso646_jp},
    {-1}, {-1},
#line 296 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str493, ei_gb2312},
    {-1},
#line 99 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str495, ei_iso8859_6},
    {-1}, {-1},
#line 173 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str498, ei_cp1250},
    {-1}, {-1}, {-1},
#line 336 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str502, ei_gb18030_2005},
    {-1}, {-1}, {-1}, {-1},
#line 170 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str507, ei_koi8_u},
#line 171 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str508, ei_koi8_ru},
    {-1},
#line 254 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str510, ei_tis620},
#line 190 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str511, ei_cp1255},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 55 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str517, ei_iso8859_1},
#line 227 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str518, ei_mac_arabic},
    {-1},
#line 96 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str520, ei_iso8859_6},
#line 215 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str521, ei_mac_roman},
    {-1}, {-1}, {-1},
#line 207 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str525, ei_cp862},
#line 113 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str526, ei_iso8859_7},
#line 103 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str527, ei_iso8859_7},
#line 104 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str528, ei_iso8859_7},
    {-1},
#line 211 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str530, ei_cp866},
#line 65 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str531, ei_iso8859_2},
    {-1}, {-1},
#line 217 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str534, ei_mac_roman},
    {-1}, {-1}, {-1},
#line 278 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str538, ei_jisx0208},
    {-1}, {-1}, {-1},
#line 221 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str542, ei_mac_romania},
#line 106 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str543, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 237 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str549, ei_koi8_t},
    {-1}, {-1},
#line 307 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str552, ei_euc_jp},
#line 306 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str553, ei_euc_jp},
#line 358 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str554, ei_big5hkscs2008},
#line 357 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str555, ei_big5hkscs2008},
    {-1},
#line 218 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str557, ei_mac_centraleurope},
    {-1}, {-1},
#line 228 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str560, ei_mac_thai},
#line 18 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str561, ei_ascii},
    {-1}, {-1}, {-1},
#line 308 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str565, ei_euc_jp},
    {-1}, {-1},
#line 344 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str568, ei_euc_tw},
#line 343 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str569, ei_euc_tw},
    {-1}, {-1},
#line 119 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str572, ei_iso8859_8},
#line 39 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str573, ei_utf16be},
#line 291 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str574, ei_iso646_cn},
    {-1},
#line 17 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str576, ei_ascii},
#line 132 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str577, ei_iso8859_10},
    {-1}, {-1},
#line 97 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str580, ei_iso8859_6},
#line 280 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str581, ei_jisx0208},
    {-1}, {-1}, {-1},
#line 202 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str585, ei_cp850},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 293 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str593, ei_iso646_cn},
    {-1}, {-1},
#line 267 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str596, ei_iso646_jp},
#line 143 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str597, ei_iso8859_13},
#line 111 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str598, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 112 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str604, ei_iso8859_7},
    {-1},
#line 270 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str606, ei_iso646_jp},
    {-1}, {-1}, {-1},
#line 42 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str610, ei_utf32be},
    {-1}, {-1},
#line 193 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str613, ei_cp1256},
#line 265 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str614, ei_tcvn},
    {-1}, {-1}, {-1},
#line 44 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str618, ei_utf7},
    {-1}, {-1}, {-1}, {-1},
#line 320 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str623, ei_iso2022_jp1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 323 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str633, ei_iso2022_jpms},
    {-1}, {-1}, {-1},
#line 321 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str637, ei_iso2022_jp2},
    {-1}, {-1}, {-1}, {-1},
#line 195 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str642, ei_cp1257},
    {-1}, {-1}, {-1}, {-1},
#line 233 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str647, ei_nextstep},
#line 258 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str648, ei_cp874},
#line 322 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str649, ei_iso2022_jp2},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 288 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str655, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 363 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str666, ei_euc_kr},
    {-1}, {-1},
#line 345 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str669, ei_euc_tw},
    {-1},
#line 318 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str671, ei_iso2022_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 32 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str679, ei_ucs2le},
#line 350 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str680, ei_ces_big5},
#line 349 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str681, ei_ces_big5},
#line 118 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str682, ei_iso8859_8},
#line 274 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str683, ei_jisx0201},
#line 319 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str684, ei_iso2022_jp},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 28 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str690, ei_ucs2be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 105 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str703, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 20 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str709, ei_ascii},
    {-1},
#line 283 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str711, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 354 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str719, ei_big5hkscs1999},
    {-1},
#line 355 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str721, ei_big5hkscs2001},
    {-1}, {-1}, {-1},
#line 271 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str725, ei_jisx0201},
    {-1}, {-1},
#line 356 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str728, ei_big5hkscs2004},
    {-1}, {-1}, {-1}, {-1},
#line 272 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str733, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 359 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str739, ei_big5hkscs2008},
    {-1}, {-1}, {-1}, {-1},
#line 223 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str744, ei_mac_ukraine},
    {-1},
#line 285 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str746, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 275 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str761, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 276 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str776, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 277 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str792, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 184 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str807, ei_cp1253},
    {-1},
#line 224 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str809, ei_mac_greek},
    {-1}, {-1}, {-1}, {-1},
#line 366 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str814, ei_johab},
#line 225 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str815, ei_mac_turkish},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 187 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str826, ei_cp1254},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 312 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str852, ei_sjis},
#line 311 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str853, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 315 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str897, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 284 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str922, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 310 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str947, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 226 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str958, ei_mac_hebrew},
    {-1}, {-1}, {-1}, {-1},
#line 346 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str963, ei_euc_tw},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 314 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str978, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 309 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str984, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 196 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1026, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 282 "lib/aliases_sysaix.gperf"
    {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str1039, ei_jisx0208}
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
