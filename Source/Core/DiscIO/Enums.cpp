// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Enums.h"

namespace DiscIO
{
bool IsDisc(Platform volume_type)
{
  return volume_type == Platform::GAMECUBE_DISC || volume_type == Platform::WII_DISC;
}

bool IsWii(Platform volume_type)
{
  return volume_type == Platform::WII_DISC || volume_type == Platform::WII_WAD;
}

bool IsNTSC(Region region)
{
  return region == Region::NTSC_J || region == Region::NTSC_U || region == Region::NTSC_K;
}

// Increment CACHE_REVISION (GameListCtrl.cpp) if the code below is modified

Country TypicalCountryForRegion(Region region)
{
  switch (region)
  {
  case Region::NTSC_J:
    return Country::COUNTRY_JAPAN;
  case Region::NTSC_U:
    return Country::COUNTRY_USA;
  case Region::PAL:
    return Country::COUNTRY_EUROPE;
  case Region::NTSC_K:
    return Country::COUNTRY_KOREA;
  default:
    return Country::COUNTRY_UNKNOWN;
  }
}

Region RegionSwitchGC(u8 country_code)
{
  Region region = RegionSwitchWii(country_code);
  return region == Region::NTSC_K ? Region::NTSC_J : region;
}

Region RegionSwitchWii(u8 country_code)
{
  switch (country_code)
  {
  case 'J':
  case 'W':
    return Region::NTSC_J;

  case 'B':
  case 'E':
  case 'N':
  case 'Z':
    return Region::NTSC_U;

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
  case 'X':
  case 'Y':
    return Region::PAL;

  case 'K':
  case 'Q':
  case 'T':
    return Region::NTSC_K;

  default:
    return Region::UNKNOWN_REGION;
  }
}

Country CountrySwitch(u8 country_code)
{
  switch (country_code)
  {
  // Worldwide
  case 'A':
    return Country::COUNTRY_WORLD;

  // PAL
  case 'D':
    return Country::COUNTRY_GERMANY;

  case 'X':  // Used by a couple PAL games
  case 'Y':  // German, French
  case 'L':  // Japanese import to PAL regions
  case 'M':  // Japanese import to PAL regions
  case 'P':
    return Country::COUNTRY_EUROPE;

  case 'U':
    return Country::COUNTRY_AUSTRALIA;

  case 'F':
    return Country::COUNTRY_FRANCE;

  case 'I':
    return Country::COUNTRY_ITALY;

  case 'H':
    return Country::COUNTRY_NETHERLANDS;

  case 'R':
    return Country::COUNTRY_RUSSIA;

  case 'S':
    return Country::COUNTRY_SPAIN;

  // NTSC
  case 'E':
  case 'N':  // Japanese import to USA and other NTSC regions
  case 'Z':  // Prince of Persia - The Forgotten Sands (Wii)
  case 'B':  // Ufouria: The Saga (Virtual Console)
    return Country::COUNTRY_USA;

  case 'J':
    return Country::COUNTRY_JAPAN;

  case 'K':
  case 'Q':  // Korea with Japanese language
  case 'T':  // Korea with English language
    return Country::COUNTRY_KOREA;

  case 'W':
    return Country::COUNTRY_TAIWAN;

  default:
    if (country_code > 'A')  // Silently ignore IOS wads
      WARN_LOG(DISCIO, "Unknown Country Code! %c", country_code);
    return Country::COUNTRY_UNKNOWN;
  }
}

Region GetSysMenuRegion(u16 title_version)
{
  if (title_version == 33)
    return Region::UNKNOWN_REGION;  // 1.0 uses 33 as the version number in all regions

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
    return Region::UNKNOWN_REGION;
  }
}

std::string GetSysMenuVersionString(u16 title_version)
{
  if (title_version == 33)
    return "1.0";  // 1.0 uses 33 as the version number in all regions

  std::string region_letter;

  switch (GetSysMenuRegion(title_version))
  {
  case Region::NTSC_J:
    region_letter = "J";
    break;
  case Region::NTSC_U:
    region_letter = "U";
    break;
  case Region::PAL:
    region_letter = "E";
    break;
  case Region::NTSC_K:
    region_letter = "K";
    break;
  case Region::UNKNOWN_REGION:
    WARN_LOG(DISCIO, "Unknown region for Wii Menu version %u", title_version);
    break;
  }

  switch (title_version & ~0xf)
  {
  case 96:
  case 128:
    return "2.0" + region_letter;
  case 160:
    return "2.1" + region_letter;
  case 192:
    return "2.2" + region_letter;
  case 224:
    return "3.0" + region_letter;
  case 256:
    return "3.1" + region_letter;
  case 288:
    return "3.2" + region_letter;
  case 320:
  case 352:
    return "3.3" + region_letter;
  case 384:
    return (region_letter != "K" ? "3.4" : "3.5") + region_letter;
  case 416:
    return "4.0" + region_letter;
  case 448:
    return "4.1" + region_letter;
  case 480:
    return "4.2" + region_letter;
  case 512:
    return "4.3" + region_letter;
  default:
    return "?.?" + region_letter;
  }
}

std::string GetCompanyFromID(const std::string& company_id)
{
  static const std::map<std::string, std::string> companies = {
      {"01", "Nintendo"},
      {"02", "Nintendo"},
      {"08", "Capcom"},
      {"0A", "Jaleco Entertainment"},
      {"0M", "Entertainment Software Publishing"},
      {"0Q", "IE Institute"},
      {"13", "Electronic Arts / EA Sports"},
      {"18", "Hudson Entertainment"},
      {"1K", "Titus Software"},
      {"20", "DSI Games / ZOO Digital Publishing"},
      {"28", "Kemco"},
      {"29", "SETA"},
      {"2K", "NEC Interchannel"},
      {"2L", "Agatsuma Entertainment"},
      {"2M", "Jorudan"},
      {"2N", "Rocket Company"},
      {"2Q", "MediaKite"},
      {"36", "Codemasters"},
      {"41", "Ubisoft"},
      {"4F", "Eidos Interactive"},
      {"4Q", "Disney Interactive Studios / Buena Vista Games"},
      {"4Z", "Crave Entertainment / Red Wagon Games"},
      {"51", "Acclaim Entertainment"},
      {"52", "Activision / Activision Value / RedOctane"},
      {"54", "Rockstar Games / 2K Play / Global Star Software"},
      {"5D", "Midway Games"},
      {"5G", "Majesco Entertainment"},
      {"5H", "3DO / Global Star Software"},
      {"5L", "NewKidCo"},
      {"5S", "Evolved Games / Xicat Interactive"},
      {"5V", "Agetec"},
      {"5Z", "Conspiracy Entertainment"},
      {"60", "Titus Software"},
      {"64", "LucasArts"},
      {"68", "Bethesda Softworks / Mud Duck Productions / Vir2L Studios"},
      {"69", "Electronic Arts / EA Sports / MTV Games"},
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
      {"70", "Atari"},
      {"71", "Interplay Entertainment"},
      {"75", "SCi"},
      {"78", "THQ / Play THQ"},
      {"7D", "Sierra Entertainment / Vivendi Games / Universal Interactive Studios"},
      {"7F", "Kemco"},
      {"7G", "Rage Software"},
      {"7H", "Encore Software"},
      {"7J", "Zushi Games / ZOO Digital Publishing"},
      {"7K", "Kiddinx Entertainment"},
      {"7L", "Simon & Schuster Interactive"},
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
      {"9G", "Take-Two Interactive / Gotham Games / Gathering of Developers"},
      {"A4", "Konami"},
      {"A7", "Takara / Takara Tomy"},
      {"AF", "Namco Bandai Games"},
      {"AU", "Alternative Software"},
      {"B0", "Acclaim Entertainment"},
      {"B2", "Bandai Games"},
      {"BB", "Gaijinworks"},
      {"BL", "MTO"},
      {"BN", "Sunrise Interactive"},
      {"BP", "Global A Entertainment"},
      {"C0", "Taito"},
      {"C8", "Koei"},
      {"CM", "Konami"},
      {"CQ", "From Software"},
      {"D9", "Banpresto"},
      {"DA", "Takara Tomy"},
      {"DQ", "Compile Heart / Idea Factory"},
      {"E5", "Epoch"},
      {"E7", "Athena"},
      {"E8", "Asmik Ace Entertainment"},
      {"E9", "Natsume"},
      {"EB", "Atlus"},
      {"EL", "Spike"},
      {"EM", "Konami"},
      {"EP", "Sting Entertainment"},
      {"ES", "Starfish SD"},
      {"EY", "Vblank Entertainment"},
      {"FH", "Easy Interactive"},
      {"FJ", "Virtual Toys"},
      {"FK", "The Game Factory"},
      {"FP", "Mastiff"},
      {"FR", "Digital Tainment Pool"},
      {"FS", "XS Games"},
      {"G0", "Alpha Unit"},
      {"G2", "Yuke's"},
      {"G9", "D3 Publisher"},
      {"GA", "PIN Change"},
      {"GD", "Square Enix"},
      {"GE", "Kids Station"},
      {"GG", "O3 Entertainment"},
      {"GJ", "Detn8"},
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
      {"HF", "Level-5"},
      {"HG", "Graffiti Entertainment"},
      {"HH", "Focus Home Interactive"},
      {"HJ", "Genius Products"},
      {"HL", "Frontier Developments"},
      {"HM", "HMH Interactive"},
      {"HN", "High Voltage Software"},
      {"HQ", "Abstraction Games"},
      {"HS", "Tru Blu"},
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
      {"JR", "Engine Software"},
      {"JS", "Digital Leisure"},
      {"JT", "Empty Clip Studios"},
      {"JW", "BigBen Interactive"},
      {"JX", "Shin'en Multimedia"},
      {"JZ", "505 Games"},
      {"K3", "Yudo"},
      {"K6", "Nihon System"},
      {"KB", "Nippon Ichi Software America"},
      {"KG", "Kando Games"},
      {"KJ", "Studio Zan"},
      {"KL", "Abylight"},
      {"KM", "Deep Silver"},
      {"KP", "Purple Hills"},
      {"KQ", "Over the Top Games"},
      {"KR", "KREA Medie"},
      {"KW", "Semnat Studios"},
      {"L3", "G-Mode"},
      {"LG", "Black Bean Games"},
      {"LJ", "Legendo Entertainment"},
      {"LL", "HB Studios"},
      {"LP", "Left Field Productions"},
      {"LR", "Koch Media"},
      {"LT", "Legacy Interactive"},
      {"LY", "Icon Games"},
      {"MD", "Ateam"},
      {"MH", "Mentor Interactive"},
      {"MJ", "Mumbo Jumbo"},
      {"ML", "DTP Young Entertainment"},
      {"MM", "Big John Games"},
      {"MR", "Mindscape"},
      {"MS", "Milestone / UFO Interactive Games"},
      {"MT", "Blast! Entertainment"},
      {"MV", "Marvelous Entertainment"},
      {"MZ", "Mad Catz"},
      {"N9", "Tera Box"},
      {"NG", "Nordic Games"},
      {"NH", "Gevo Entertainment"},
      {"NJ", "Enjoy Gaming"},
      {"NK", "Neko Entertainment"},
      {"NL", "Nordic Softsales"},
      {"NP", "Nobilis"},
      {"NQ", "Namco Bandai Partners"},
      {"NR", "Bold Games / Destineer Games"},
      {"NS", "Nippon Ichi Software"},
      {"NV", "Nicalis"},
      {"NW", "Deep Fried Entertainment"},
      {"NX", "Barnstorm Games"},
      {"NY", "Nicalis"},
      {"PH", "Playful Entertainment"},
      {"PK", "Knowledge Adventure"},
      {"PL", "Playlogic Entertainment"},
      {"PM", "Warner Bros. Interactive Entertainment"},
      {"PN", "P2 Games"},
      {"PQ", "PopCap Games"},
      {"PZ", "GameMill Publishing"},
      {"Q4", "Hamster"},
      {"QC", "Kadokawa Shoten / Enterbrain"},
      {"QH", "Virtual Play Games"},
      {"QK", "MACHINE Studios"},
      {"QM", "Object Vision Software"},
      {"QT", "CALARIS"},
      {"QU", "QubicGames"},
      {"QX", "Press Play"},
      {"RA", "Office Create"},
      {"RG", "Ronimo Games"},
      {"RM", "Rondomedia"},
      {"RN", "Mastiff / N3V Games"},
      {"RS", "Brash Entertainment"},
      {"RT", "RTL Enterprises"},
      {"RV", "bitComposer Games"},
      {"RW", "RealArcade"},
      {"RZ", "Akaoni Studio"},
      {"S5", "SouthPeak Games"},
      {"SJ", "Cosmonaut Games"},
      {"SP", "Blade Interactive Studios"},
      {"SQ", "Sonalysts"},
      {"SU", "Slitherine"},
      {"SV", "7G//AMES"},
      {"SZ", "Storm City Games"},
      {"TJ", "Broken Rules"},
      {"TL", "Telltale Games"},
      {"TR", "Tetris Online"},
      {"TV", "Tivola"},
      {"TW", "Two Tribes"},
      {"TY", "Teyon"},
      {"UG", "Data Design Interactive / Popcorn Arcade / Metro 3D"},
      {"UH", "Intenium Console"},
      {"UJ", "Ghostlight"},
      {"UN", "Chillingo"},
      {"UP", "EnjoyUp Games"},
      {"UR", "Sudden Games"},
      {"UU", "Onteca"},
      {"UW", "Coresoft"},
      {"VL", "Eko Software"},
      {"VN", "Valcon Games"},
      {"VP", "Virgin Play"},
      {"VS", "Korner Entertainment"},
      {"VT", "Microforum Games"},
      {"VV", "Pixonauts"},
      {"VZ", "Little Orbit"},
      {"WG", "2D Boy"},
      {"WH", "NinjaBee"},
      {"WJ", "Studio Walljump"},
      {"WN", "tons of bits"},
      {"WR", "Warner Bros. Interactive Entertainment"},
      {"WS", "MonkeyPaw Games"},
      {"WW", "Slang Publishing"},
      {"WY", "WayForward Technologies"},
      {"WZ", "Wizarbox"},
      {"X3", "CK Games"},
      {"X4", "Easy Interactive"},
      {"XG", "XGen Studios"},
      {"XJ", "XSEED Games"},
      {"XK", "Exkee"},
      {"XM", "DreamBox Games"},
      {"XS", "Aksys Games"},
      {"XT", "Funbox Media"},
      {"XV", "Keystone Game Studio"},
      {"XW", "Lemon Games"},
      {"Y1", "Tubby Games"},
      {"Y5", "Easy Interactive"},
      {"Y6", "Motiviti"},
      {"YC", "NECA"},
      {"YD", "Infinite Dreams"},
      {"YF", "Oxygene"},
      {"YG", "Maximum Family Games"},
      {"YT", "Valcon Games"},
      {"YY", "FDG Entertainment"},
      {"Z4", "Ntreev Soft"},
      {"ZA", "WBA Interactive"},
      {"ZG", "Zallag"},
      {"ZH", "Internal Engine"},
      {"ZJ", "Performance Designed Products"},
      {"ZK", "Anima Game Studio"},
      {"ZS", "Zinkia Entertainment"},
      {"ZW", "Judo Baby"},
      {"ZX", "TopWare Interactive"}};

  auto iterator = companies.find(company_id);
  if (iterator != companies.end())
    return iterator->second;
  else
    return "";
}
}
