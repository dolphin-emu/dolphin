// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/Enums.h"

#include <map>
#include <string>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

namespace DiscIO
{
std::string GetName(Country country, bool translate)
{
  std::string name;

  switch (country)
  {
  case Country::Europe:
    name = _trans("Europe");
    break;
  case Country::Japan:
    name = _trans("Japan");
    break;
  case Country::USA:
    name = _trans("USA");
    break;
  case Country::Australia:
    name = _trans("Australia");
    break;
  case Country::France:
    name = _trans("France");
    break;
  case Country::Germany:
    name = _trans("Germany");
    break;
  case Country::Italy:
    name = _trans("Italy");
    break;
  case Country::Korea:
    name = _trans("Korea");
    break;
  case Country::Netherlands:
    name = _trans("Netherlands");
    break;
  case Country::Russia:
    name = _trans("Russia");
    break;
  case Country::Spain:
    name = _trans("Spain");
    break;
  case Country::Taiwan:
    name = _trans("Taiwan");
    break;
  case Country::World:
    name = _trans("World");
    break;
  default:
    name = _trans("Unknown");
    break;
  }

  return translate ? Common::GetStringT(name.c_str()) : name;
}

std::string GetName(Language language, bool translate)
{
  std::string name;

  switch (language)
  {
  case Language::Japanese:
    name = _trans("Japanese");
    break;
  case Language::English:
    name = _trans("English");
    break;
  case Language::German:
    name = _trans("German");
    break;
  case Language::French:
    name = _trans("French");
    break;
  case Language::Spanish:
    name = _trans("Spanish");
    break;
  case Language::Italian:
    name = _trans("Italian");
    break;
  case Language::Dutch:
    name = _trans("Dutch");
    break;
  case Language::SimplifiedChinese:
    name = _trans("Simplified Chinese");
    break;
  case Language::TraditionalChinese:
    name = _trans("Traditional Chinese");
    break;
  case Language::Korean:
    name = _trans("Korean");
    break;
  default:
    name = _trans("Unknown");
    break;
  }

  return translate ? Common::GetStringT(name.c_str()) : name;
}

bool IsDisc(Platform volume_type)
{
  return volume_type == Platform::GameCubeDisc || volume_type == Platform::WiiDisc;
}

bool IsWii(Platform volume_type)
{
  return volume_type == Platform::WiiDisc || volume_type == Platform::WiiWAD;
}

bool IsNTSC(Region region)
{
  return region == Region::NTSC_J || region == Region::NTSC_U || region == Region::NTSC_K;
}

int ToGameCubeLanguage(Language language)
{
  if (language < Language::English || language > Language::Dutch)
    return 0;
  else
    return static_cast<int>(language) - 1;
}

Language FromGameCubeLanguage(int language)
{
  if (language < 0 || language > 5)
    return Language::Unknown;
  else
    return static_cast<Language>(language + 1);
}

// Increment CACHE_REVISION (GameFileCache.cpp) if the code below is modified

Country TypicalCountryForRegion(Region region)
{
  switch (region)
  {
  case Region::NTSC_J:
    return Country::Japan;
  case Region::NTSC_U:
    return Country::USA;
  case Region::PAL:
    return Country::Europe;
  case Region::NTSC_K:
    return Country::Korea;
  default:
    return Country::Unknown;
  }
}

Region SysConfCountryToRegion(u8 country_code)
{
  if (country_code == 0)
    return Region::Unknown;

  if (country_code < 0x08)  // Japan
    return Region::NTSC_J;

  if (country_code < 0x40)  // Americas
    return Region::NTSC_U;

  if (country_code < 0x80)  // Europe, Oceania, parts of Africa
    return Region::PAL;

  if (country_code < 0xa8)  // Southeast Asia
    return country_code == 0x88 ? Region::NTSC_K : Region::NTSC_J;

  if (country_code < 0xc0)  // Middle East
    return Region::NTSC_U;

  return Region::Unknown;
}

Region CountryCodeToRegion(u8 country_code, Platform platform, Region expected_region,
                           std::optional<u16> revision)
{
  switch (country_code)
  {
  case '\2':
    return expected_region;  // Wii Menu (same title ID for all regions)

  case 'J':
    return Region::NTSC_J;

  case 'W':
    if (expected_region == Region::PAL)
      return Region::PAL;  // Only the Nordic version of Ratatouille (Wii)
    else
      return Region::NTSC_J;  // Korean GC games in English or Taiwanese Wii games

  case 'E':
    if (platform != Platform::GameCubeDisc)
      return Region::NTSC_U;  // The most common country code for NTSC-U

    if (revision)
    {
      if (*revision >= 0x30)
        return Region::NTSC_J;  // Korean GC games in English
      else
        return Region::NTSC_U;  // The most common country code for NTSC-U
    }
    else
    {
      if (expected_region == Region::NTSC_J)
        return Region::NTSC_J;  // Korean GC games in English
      else
        return Region::NTSC_U;  // The most common country code for NTSC-U
    }

  case 'B':
  case 'N':
    return Region::NTSC_U;

  case 'X':
  case 'Y':
  case 'Z':
    // Additional language versions, store-exclusive versions, other special versions
    return expected_region == Region::NTSC_U ? Region::NTSC_U : Region::PAL;

  case 'D':
  case 'F':
  case 'H':
  case 'I':
  case 'L':
  case 'M':
  case 'P':
  case 'R':
  case 'S':
  case 'U':
  case 'V':
    return Region::PAL;

  case 'K':
  case 'Q':
  case 'T':
    // All of these country codes are Korean, but the NTSC-K region doesn't exist on GC
    return platform == Platform::GameCubeDisc ? Region::NTSC_J : Region::NTSC_K;

  default:
    return Region::Unknown;
  }
}

Country CountryCodeToCountry(u8 country_code, Platform platform, Region region,
                             std::optional<u16> revision)
{
  switch (country_code)
  {
  // Worldwide
  case 'A':
    return Country::World;

  // Mixed regions
  case 'X':
  case 'Y':
  case 'Z':
    // Additional language versions, store-exclusive versions, other special versions
    return region == Region::NTSC_U ? Country::USA : Country::Europe;

  case 'W':
    if (platform == Platform::GameCubeDisc)
      return Country::Korea;  // GC games in English released in Korea
    else if (region == Region::PAL)
      return Country::Europe;  // Only the Nordic version of Ratatouille (Wii)
    else
      return Country::Taiwan;  // Wii games in traditional Chinese released in Taiwan

  // PAL
  case 'D':
    return Country::Germany;

  case 'L':  // NTSC-J games released on PAL VC
  case 'M':  // NTSC-U games released on PAL VC
  case 'V':  // Used by some Nordic Wii releases
  case 'P':  // The most common country code for PAL
    return Country::Europe;

  case 'U':
    return Country::Australia;

  case 'F':
    return Country::France;

  case 'I':
    return Country::Italy;

  case 'H':
    return Country::Netherlands;

  case 'R':
    return Country::Russia;

  case 'S':
    return Country::Spain;

  // NTSC
  case 'E':
    if (platform != Platform::GameCubeDisc)
      return Country::USA;  // The most common country code for NTSC-U

    if (revision)
    {
      if (*revision >= 0x30)
        return Country::Korea;  // GC games in English released in Korea
      else
        return Country::USA;  // The most common country code for NTSC-U
    }
    else
    {
      if (region == Region::NTSC_J)
        return Country::Korea;  // GC games in English released in Korea
      else
        return Country::USA;  // The most common country code for NTSC-U
    }

  case 'B':  // PAL games released on NTSC-U VC
  case 'N':  // NTSC-J games released on NTSC-U VC
    return Country::USA;

  case 'J':
    return Country::Japan;

  case 'K':  // Games in Korean released in Korea
  case 'Q':  // NTSC-J games released on NTSC-K VC
  case 'T':  // NTSC-U games released on NTSC-K VC
    return Country::Korea;

  default:
    if (country_code > 'A')  // Silently ignore IOS wads
      WARN_LOG_FMT(DISCIO, "Unknown Country Code! {}", static_cast<char>(country_code));
    return Country::Unknown;
  }
}

Region GetSysMenuRegion(u16 title_version)
{
  switch (title_version & 0xf)
  {
  case 0:
    return Region::NTSC_J;
  case 1:
    return Region::NTSC_U;
  case 2:
    return Region::PAL;
  case 6:
    return Region::NTSC_K;
  default:
    return Region::Unknown;
  }
}

std::string GetSysMenuVersionString(u16 title_version, bool is_vwii)
{
  std::string version;
  char region_letter = '\0';

  switch (GetSysMenuRegion(title_version))
  {
  case Region::NTSC_J:
    region_letter = 'J';
    break;
  case Region::NTSC_U:
    region_letter = 'U';
    break;
  case Region::PAL:
    region_letter = 'E';
    break;
  case Region::NTSC_K:
    region_letter = 'K';
    break;
  case Region::Unknown:
    WARN_LOG_FMT(DISCIO, "Unknown region for Wii Menu version {}", title_version);
    break;
  }

  if (is_vwii)
  {
    // For vWii return the Wii U version which installed the menu
    switch (title_version & 0xff0)
    {
    case 512:
      version = "1.0.0";
      break;
    case 544:
      version = "4.0.0";
      break;
    case 608:
      version = "5.2.0";
      break;
    default:
      version = "?.?.?";
      break;
    }
  }
  else
  {
    switch (title_version & 0xff0)
    {
    case 32:
      version = "1.0";
      break;
    case 96:
    case 128:
      version = "2.0";
      break;
    case 160:
      version = "2.1";
      break;
    case 192:
      version = "2.2";
      break;
    case 224:
      version = "3.0";
      break;
    case 256:
      version = "3.1";
      break;
    case 288:
      version = "3.2";
      break;
    case 320:
    case 352:
      version = "3.3";
      break;
    case 384:
      version = (region_letter != 'K' ? "3.4" : "3.5");
      break;
    case 416:
      version = "4.0";
      break;
    case 448:
      version = "4.1";
      break;
    case 480:
      version = "4.2";
      break;
    case 512:
      version = "4.3";
      break;
    default:
      version = "?.?";
      break;
    }
  }

  if (region_letter != '\0')
    version += region_letter;

  return version;
}

const std::string& GetCompanyFromID(const std::string& company_id)
{
  static const std::map<std::string, std::string> companies = {
      {"01", "Nintendo"},
      {"02", "Nintendo"},
      {"08", "Capcom"},
      {"0A", "Jaleco / Jaleco Entertainment"},
      {"0L", "Warashi"},
      {"0M", "Entertainment Software Publishing"},
      {"0Q", "IE Institute"},
      {"13", "Electronic Arts Japan"},
      {"18", "Hudson Soft / Hudson Entertainment"},
      {"1K", "Titus Software"},
      {"20", "DSI Games / ZOO Digital Publishing"},
      {"28", "Kemco Japan"},
      {"29", "SETA Corporation"},
      {"2K", "NEC Interchannel"},
      {"2L", "Agatsuma Entertainment"},
      {"2M", "Jorudan"},
      {"2N", "Smilesoft / Rocket Company"},
      {"2Q", "MediaKite"},
      {"36", "Codemasters"},
      {"41", "Ubisoft"},
      {"4F", "Eidos Interactive"},
      {"4Q", "Disney Interactive Studios / Buena Vista Games"},
      {"4Z", "Crave Entertainment / Red Wagon Games"},
      {"51", "Acclaim Entertainment"},
      {"52", "Activision"},
      {"54", "Take-Two Interactive / GameTek / Rockstar Games / Global Star Software"},
      {"5D", "Midway Games / Tradewest"},
      {"5G", "Majesco Entertainment"},
      {"5H", "3DO / Global Star Software"},
      {"5L", "NewKidCo"},
      {"5S", "Evolved Games / Xicat Interactive"},
      {"5V", "Agetec"},
      {"5Z", "Data Design / Conspiracy Entertainment"},
      {"60", "Titus Interactive / Titus Software"},
      {"64", "LucasArts"},
      {"68", "Bethesda Softworks / Mud Duck Productions / Vir2L Studios"},
      {"69", "Electronic Arts"},
      {"6E", "Sega"},
      {"6K", "UFO Interactive Games"},
      {"6L", "BAM! Entertainment"},
      {"6M", "System 3"},
      {"6N", "Midas Interactive Entertainment"},
      {"6S", "TDK Mediactive"},
      {"6U", "The Adventure Company / DreamCatcher Interactive"},
      {"6V", "JoWooD Entertainment"},
      {"6W", "Sega"},
      {"6X", "Wanadoo Edition"},
      {"6Z", "NDS Software"},
      {"70", "Atari (Infogrames)"},
      {"71", "Interplay Entertainment"},
      {"75", "SCi Games"},
      {"78", "THQ / Play THQ"},
      {"7D", "Sierra Entertainment / Vivendi Games / Universal Interactive Studios"},
      {"7F", "Kemco"},
      {"7G", "Rage Software"},
      {"7H", "Encore Software"},
      {"7J", "Zushi Games / ZOO Digital Publishing"},
      {"7K", "Kiddinx Entertainment"},
      {"7L", "Simon & Schuster Interactive"},
      {"7M", "Badland Games"},
      {"7N", "Empire Interactive / Xplosiv"},
      {"7S", "Rockstar Games"},
      {"7T", "Scholastic"},
      {"7U", "Ignition Entertainment"},
      {"82", "Namco"},
      {"8G", "NEC Interchannel"},
      {"8J", "Kadokawa Shoten"},
      {"8M", "CyberFront"},
      {"8N", "Success"},
      {"8P", "Sega"},
      {"91", "Chunsoft"},
      {"99", "Marvelous Entertainment / Victor Entertainment / Pack-In-Video / Rising Star Games"},
      {"9B", "Tecmo"},
      {"9G",
       "Take-Two Interactive / Global Star Software / Gotham Games / Gathering of Developers"},
      {"9S", "Brother International"},
      {"9Z", "Crunchyroll"},
      {"A4", "Konami"},
      {"A7", "Takara"},
      {"AF", "Namco Bandai Games"},
      {"AU", "Alternative Software"},
      {"AX", "Vivendi"},
      {"B0", "Acclaim Japan"},
      {"B2", "Bandai Games"},
      {"BB", "Sunsoft"},
      {"BL", "MTO"},
      {"BM", "XING"},
      {"BN", "Sunrise Interactive"},
      {"BP", "Global A Entertainment"},
      {"C0", "Taito"},
      {"C8", "Koei"},
      {"CM", "Konami Computer Entertainment Osaka"},
      {"CQ", "From Software"},
      {"D9", "Banpresto"},
      {"DA", "Tomy / Takara Tomy"},
      {"DQ", "Compile Heart / Idea Factory"},
      {"E5", "Epoch"},
      {"E6", "Game Arts"},
      {"E7", "Athena"},
      {"E8", "Asmik Ace Entertainment"},
      {"E9", "Natsume"},
      {"EB", "Atlus"},
      {"EL", "Spike"},
      {"EM", "Konami Computer Entertainment Tokyo"},
      {"EP", "Sting Entertainment"},
      {"ES", "Starfish-SD"},
      {"EY", "Vblank Entertainment"},
      {"FH", "Easy Interactive"},
      {"FJ", "Virtual Toys"},
      {"FK", "The Game Factory"},
      {"FP", "Mastiff"},
      {"FR", "Digital Tainment Pool"},
      {"FS", "XS Games"},
      {"G0", "Alpha Unit"},
      {"G2", "Yuke's"},
      {"G6", "SIMS"},
      {"G9", "D3 Publisher"},
      {"GA", "PIN Change"},
      {"GD", "Square Enix"},
      {"GE", "Kids Station"},
      {"GG", "O3 Entertainment"},
      {"GJ", "Detn8 Games"},
      {"GK", "Genki"},
      {"GL", "Gameloft / Ubisoft"},
      {"GM", "Gamecock Media Group"},
      {"GN", "Oxygen Games"},
      {"GR", "GSP"},
      {"GT", "505 Games"},
      {"GX", "Commodore"},
      {"GY", "The Game Factory"},
      {"GZ", "Gammick Entertainment"},
      {"H3", "Zen United"},
      {"H4", "SNK Playmore"},
      {"HA", "Nobilis"},
      {"HE", "Gust"},
      {"HF", "Level-5"},
      {"HG", "Graffiti Entertainment"},
      {"HH", "Focus Home Interactive"},
      {"HJ", "Genius Products"},
      {"HK", "D2C Games"},
      {"HL", "Frontier Developments"},
      {"HM", "HMH Interactive"},
      {"HN", "High Voltage Software"},
      {"HQ", "Abstraction Games"},
      {"HS", "Tru Blu"},
      {"HT", "Big Blue Bubble"},
      {"HU", "Ghostfire Games"},
      {"HW", "Incredible Technologies"},
      {"HY", "Reef Entertainment"},
      {"HZ", "Nordcurrent"},
      {"J8", "D4 Enterprise"},
      {"J9", "AQ Interactive"},
      {"JD", "SKONEC Entertainment"},
      {"JE", "E Frontier"},
      {"JF", "Arc System Works"},
      {"JG", "The Games Company"},
      {"JH", "City Interactive"},
      {"JJ", "Deep Silver"},
      {"JP", "redspotgames"},
      {"JR", "Engine Software"},
      {"JS", "Digital Leisure"},
      {"JT", "Empty Clip Studios"},
      {"JU", "Riverman Media"},
      {"JV", "JV Games"},
      {"JW", "BigBen Interactive"},
      {"JX", "Shin'en Multimedia"},
      {"JY", "Steel Penny Games"},
      {"JZ", "505 Games"},
      {"K2", "Coca-Cola (Japan) Company"},
      {"K3", "Yudo"},
      {"K6", "Nihon System"},
      {"KB", "Nippon Ichi Software"},
      {"KG", "Kando Games"},
      {"KH", "Joju Games"},
      {"KJ", "Studio Zan"},
      {"KK", "DK Games"},
      {"KL", "Abylight"},
      {"KM", "Deep Silver"},
      {"KN", "Gameshastra"},
      {"KP", "Purple Hills"},
      {"KQ", "Over the Top Games"},
      {"KR", "KREA Medie"},
      {"KT", "The Code Monkeys"},
      {"KW", "Semnat Studios"},
      {"KY", "Medaverse Studios"},
      {"L3", "G-Mode"},
      {"L8", "FujiSoft"},
      {"LB", "Tryfirst"},
      {"LD", "Studio Zan"},
      {"LF", "Kemco"},
      {"LG", "Black Bean Games"},
      {"LJ", "Legendo Entertainment"},
      {"LL", "HB Studios"},
      {"LN", "GameOn"},
      {"LP", "Left Field Productions"},
      {"LR", "Koch Media"},
      {"LT", "Legacy Interactive"},
      {"LU", "Lexis Num\xc3\xa9rique"},  // We can't use a u8 prefix due to C++20's u8string
      {"LW", "Grendel Games"},
      {"LY", "Icon Games / Super Icon"},
      {"M0", "Studio Zan"},
      {"M1", "Grand Prix Games"},
      {"M2", "HomeMedia"},
      {"M4", "Cybird"},
      {"M6", "Perpetuum"},
      {"MB", "Agenda"},
      {"MD", "Ateam"},
      {"ME", "Silver Star Japan"},
      {"MF", "Yamasa"},
      {"MH", "Mentor Interactive"},
      {"MJ", "Mumbo Jumbo"},
      {"ML", "DTP Young Entertainment"},
      {"MM", "Big John Games"},
      {"MN", "Mindscape"},
      {"MR", "Mindscape"},
      {"MS", "Milestone / UFO Interactive Games"},
      {"MT", "Blast! Entertainment"},
      {"MV", "Marvelous Entertainment"},
      {"MZ", "Mad Catz"},
      {"N0", "Exkee"},
      {"N4", "Zoom"},
      {"N7", "T&S"},
      {"N9", "Tera Box"},
      {"NA", "Tom Create"},
      {"NB", "HI Games & Publishing"},
      {"NE", "Kosaido"},
      {"NF", "Peakvox"},
      {"NG", "Nordic Games"},
      {"NH", "Gevo Entertainment"},
      {"NJ", "Enjoy Gaming"},
      {"NK", "Neko Entertainment"},
      {"NL", "Nordic Softsales"},
      {"NN", "Nnooo"},
      {"NP", "Nobilis"},
      {"NQ", "Namco Bandai Partners"},
      {"NR", "Destineer Publishing / Bold Games"},
      {"NS", "Nippon Ichi Software America"},
      {"NT", "Nocturnal Entertainment"},
      {"NV", "Nicalis"},
      {"NW", "Deep Fried Entertainment"},
      {"NX", "Barnstorm Games"},
      {"NY", "Nicalis"},
      {"P1", "Poisoft"},
      {"PH", "Playful Entertainment"},
      {"PK", "Knowledge Adventure"},
      {"PL", "Playlogic Entertainment"},
      {"PM", "Warner Bros. Interactive Entertainment"},
      {"PN", "P2 Games"},
      {"PQ", "PopCap Games"},
      {"PS", "Bplus"},
      {"PT", "Firemint"},
      {"PU", "Pub Company"},
      {"PV", "Pan Vision"},
      {"PY", "Playstos Entertainment"},
      {"PZ", "GameMill Publishing"},
      {"Q2", "Santa Entertainment"},
      {"Q3", "Asterizm"},
      {"Q4", "Hamster"},
      {"Q5", "Recom"},
      {"QA", "Miracle Kidz"},
      {"QC", "Kadokawa Shoten / Enterbrain"},
      {"QH", "Virtual Play Games"},
      {"QK", "MACHINE Studios"},
      {"QM", "Object Vision Software"},
      {"QQ", "Gamelion"},
      {"QR", "Lapland Studio"},
      {"QT", "CALARIS"},
      {"QU", "QubicGames"},
      {"QV", "Ludia"},
      {"QW", "Kaasa Solution"},
      {"QX", "Press Play"},
      {"QZ", "Hands-On Mobile"},
      {"RA", "Office Create"},
      {"RG", "Ronimo Games"},
      {"RH", "h2f Games"},
      {"RM", "Rondomedia"},
      {"RN", "Mastiff / N3V Games"},
      {"RQ", "GolemLabs & Zoozen"},
      {"RS", "Brash Entertainment"},
      {"RT", "RTL Enterprises"},
      {"RV", "bitComposer Games"},
      {"RW", "RealArcade"},
      {"RX", "Reflexive Entertainment"},
      {"RZ", "Akaoni Studio"},
      {"S5", "SouthPeak Games"},
      {"SH", "Sabarasa"},
      {"SJ", "Cosmonaut Games"},
      {"SP", "Blade Interactive Studios"},
      {"SQ", "Sonalysts"},
      {"SR", "SnapDragon Games"},
      {"SS", "Sanuk Games"},
      {"ST", "Stickmen Studios"},
      {"SU", "Slitherine"},
      {"SV", "SevenOne Intermedia"},
      {"SZ", "Storm City Games"},
      {"TH", "Kolkom"},
      {"TJ", "Broken Rules"},
      {"TL", "Telltale Games"},
      {"TR", "Tetris Online"},
      {"TS", "Triangle Studios"},
      {"TV", "Tivola"},
      {"TW", "Two Tribes"},
      {"TY", "Teyon"},
      {"UG", "Data Design Interactive / Popcorn Arcade / Metro 3D"},
      {"UH", "Intenium Console"},
      {"UJ", "Ghostlight"},
      {"UK", "iFun4all"},
      {"UN", "Chillingo"},
      {"UP", "EnjoyUp Games"},
      {"UR", "Sudden Games"},
      {"US", "USM"},
      {"UU", "Onteca"},
      {"UV", "Fugazo"},
      {"UW", "Coresoft"},
      {"VG", "Vogster Entertainment"},
      {"VK", "Sandlot Games"},
      {"VL", "Eko Software"},
      {"VN", "Valcon Games"},
      {"VP", "Virgin Play"},
      {"VS", "Korner Entertainment"},
      {"VT", "Microforum Games"},
      {"VU", "Double Jungle"},
      {"VV", "Pixonauts"},
      {"VX", "Frontline Studios"},
      {"VZ", "Little Orbit"},
      {"WD", "Amazon"},
      {"WG", "2D Boy"},
      {"WH", "NinjaBee"},
      {"WJ", "Studio Walljump"},
      {"WL", "Wired Productions"},
      {"WN", "tons of bits"},
      {"WP", "White Park Bay Software"},
      {"WQ", "Revistronic"},
      {"WR", "Warner Bros. Interactive Entertainment"},
      {"WS", "MonkeyPaw Games"},
      {"WW", "Slang Publishing"},
      {"WY", "WayForward Technologies"},
      {"WZ", "Wizarbox"},
      {"X0", "SDP Games"},
      {"X3", "CK Games"},
      {"X4", "Easy Interactive"},
      {"XB", "Hulu"},
      {"XG", "XGen Studios"},
      {"XJ", "XSEED Games"},
      {"XK", "Exkee"},
      {"XM", "DreamBox Games"},
      {"XN", "Netflix"},
      {"XS", "Aksys Games"},
      {"XT", "Funbox Media"},
      {"XU", "Shanblue Interactive"},
      {"XV", "Keystone Game Studio"},
      {"XW", "Lemon Games"},
      {"XY", "Gaijin Games"},
      {"Y1", "Tubby Games"},
      {"Y5", "Easy Interactive"},
      {"Y6", "Motiviti"},
      {"Y7", "The Learning Company"},
      {"Y9", "RadiationBurn"},
      {"YC", "NECA"},
      {"YD", "Infinite Dreams"},
      {"YF", "O2 Games"},
      {"YG", "Maximum Family Games"},
      {"YJ", "Frozen Codebase"},
      {"YK", "MAD Multimedia"},
      {"YN", "Game Factory"},
      {"YS", "Yullaby"},
      {"YT", "Corecell Technology"},
      {"YV", "KnapNok Games"},
      {"YX", "Selectsoft"},
      {"YY", "FDG Entertainment"},
      {"Z4", "Ntreev Soft"},
      {"Z5", "Shinsegae I&C"},
      {"ZA", "WBA Interactive"},
      {"ZG", "Zallag"},
      {"ZH", "Internal Engine"},
      {"ZJ", "Performance Designed Products"},
      {"ZK", "Anima Game Studio"},
      {"ZP", "Fishing Cactus"},
      {"ZS", "Zinkia Entertainment"},
      {"ZV", "RedLynx"},
      {"ZW", "Judo Baby"},
      {"ZX", "TopWare Interactive"}};

  static const std::string EMPTY_STRING;
  auto iterator = companies.find(company_id);
  if (iterator != companies.end())
    return iterator->second;
  else
    return EMPTY_STRING;
}
}  // namespace DiscIO
