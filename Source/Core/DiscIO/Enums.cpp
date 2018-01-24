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
      {"02", "Rocket Games / Ajinomoto"},
      {"03", "Imagineer-Zoom"},
      {"04", "Gray Matter"},
      {"05", "Zamuse"},
      {"06", "Falcom"},
      {"07", "Enix"},
      {"08", "Capcom"},
      {"09", "Hot B Co."},
      {"0A", "Jaleco"},
      {"0B", "Coconuts Japan"},
      {"0C", "Coconuts Japan / GX Media"},
      {"0D", "Micronet"},
      {"0E", "Technos"},
      {"0F", "Mebio Software"},
      {"0G", "Shouei System"},
      {"0H", "Starfish"},
      {"0J", "Mitsui Fudosan / Dentsu"},
      {"0L", "Warashi Inc."},
      {"0N", "Nowpro"},
      {"0P", "Game Village"},
      {"0Q", "IE Institute"},
      {"12", "Infocom"},
      {"13", "Electronic Arts Japan"},
      {"15", "Cobra Team"},
      {"16", "Human / Field"},
      {"17", "KOEI"},
      {"18", "Hudson Soft"},
      {"19", "S.C.P."},
      {"1A", "Yanoman"},
      {"1C", "Tecmo Products"},
      {"1D", "Japan Glary Business"},
      {"1E", "Forum / OpenSystem"},
      {"1F", "Virgin Games (Japan)"},
      {"1G", "SMDE"},
      {"1J", "Daikokudenki"},
      {"1P", "Creatures Inc."},
      {"1Q", "TDK Deep Impresion"},
      {"20", "Zoo"},
      {"21", "Sunsoft / Tokai Engineering"},
      {"22", "Planning Office Wada / VR1 Japan"},
      {"23", "Micro World"},
      {"25", "San-X"},
      {"26", "Enix"},
      {"27", "Loriciel / Electro Brain"},
      {"28", "Kemco Japan"},
      {"29", "Seta"},
      {"2A", "Culture Brain"},
      {"2C", "Palsoft"},
      {"2D", "Visit Co., Ltd."},
      {"2E", "Intec"},
      {"2F", "System Sacom"},
      {"2G", "Poppo"},
      {"2H", "Ubisoft Japan"},
      {"2J", "Media Works"},
      {"2K", "NEC InterChannel"},
      {"2L", "Tam"},
      {"2M", "Jordan"},
      {"2N", "Smilesoft / Rocket"},
      {"2Q", "Mediakite"},
      {"30", "Viacom"},
      {"31", "Carrozzeria"},
      {"32", "Dynamic"},
      {"34", "Magifact"},
      {"35", "Hect"},
      {"36", "Codemasters"},
      {"37", "Taito / GAGA Communications"},
      {"38", "Laguna"},
      {"39", "Telstar / Event / Taito "},
      {"3B", "Arcade Zone Ltd."},
      {"3C", "Entertainment International / Empire Software"},
      {"3D", "Loriciel"},
      {"3E", "Gremlin Graphics"},
      {"3F", "K. Amusement Leasing Co."},
      {"40", "Seika Corp."},
      {"41", "Ubisoft Entertainment"},
      {"42", "Sunsoft US"},
      {"44", "Life Fitness"},
      {"46", "System 3"},
      {"47", "Spectrum Holobyte"},
      {"49", "IREM"},
      {"4B", "Raya Systems"},
      {"4C", "Renovation Products"},
      {"4D", "Malibu Games"},
      {"4F", "Eidos"},
      {"4G", "Playmates Interactive"},
      {"4J", "Fox Interactive"},
      {"4K", "Time Warner Interactive"},
      {"4Q", "Disney Interactive"},
      {"4S", "Black Pearl"},
      {"4U", "Advanced Productions"},
      {"4X", "GT Interactive"},
      {"4Y", "Rare"},
      {"4Z", "Crave Entertainment"},
      {"50", "Absolute Entertainment"},
      {"51", "Acclaim"},
      {"52", "Activision"},
      {"53", "American Sammy"},
      {"54", "Take 2 Interactive / GameTek"},
      {"55", "Hi Tech"},
      {"56", "LJN Ltd."},
      {"58", "Mattel"},
      {"5A", "Mindscape / Red Orb Entertainment"},
      {"5B", "Romstar"},
      {"5C", "Taxan"},
      {"5D", "Midway / Tradewest"},
      {"5F", "American Softworks"},
      {"5G", "Majesco Sales Inc."},
      {"5H", "3DO"},
      {"5K", "Hasbro"},
      {"5L", "NewKidCo"},
      {"5M", "Telegames"},
      {"5N", "Metro3D"},
      {"5P", "Vatical Entertainment"},
      {"5Q", "LEGO Media"},
      {"5S", "Xicat Interactive"},
      {"5T", "Cryo Interactive"},
      {"5W", "Red Storm Entertainment"},
      {"5X", "Microids"},
      {"5Z", "Data Design / Conspiracy / Swing"},
      {"60", "Titus"},
      {"61", "Virgin Interactive"},
      {"62", "Maxis"},
      {"64", "LucasArts Entertainment"},
      {"67", "Ocean"},
      {"68", "Bethesda Softworks"},
      {"69", "Electronic Arts"},
      {"6B", "Laser Beam"},
      {"6E", "Elite Systems"},
      {"6F", "Electro Brain"},
      {"6G", "The Learning Company"},
      {"6H", "BBC"},
      {"6J", "Software 2000"},
      {"6K", "UFO Interactive Games"},
      {"6L", "BAM! Entertainment"},
      {"6M", "Studio 3"},
      {"6Q", "Classified Games"},
      {"6S", "TDK Mediactive"},
      {"6U", "DreamCatcher"},
      {"6V", "JoWood Produtions"},
      {"6W", "Sega"},
      {"6X", "Wannado Edition"},
      {"6Y", "LSP (Light & Shadow Prod.)"},
      {"6Z", "ITE Media"},
      {"70", "Atari (Infogrames)"},
      {"71", "Interplay"},
      {"72", "JVC (US)"},
      {"73", "Parker Brothers"},
      {"75", "Sales Curve (Storm / SCI)"},
      {"78", "THQ"},
      {"79", "Accolade"},
      {"7A", "Triffix Entertainment"},
      {"7C", "Microprose Software"},
      {"7D", "Sierra / Universal Interactive"},
      {"7F", "Kemco"},
      {"7G", "Rage Software"},
      {"7H", "Encore"},
      {"7J", "Zoo"},
      {"7K", "Kiddinx"},
      {"7L", "Simon & Schuster Interactive"},
      {"7M", "Asmik Ace Entertainment Inc."},
      {"7N", "Empire Interactive"},
      {"7Q", "Jester Interactive"},
      {"7S", "Rockstar Games"},
      {"7T", "Scholastic"},
      {"7U", "Ignition Entertainment"},
      {"7V", "Summitsoft"},
      {"7W", "Stadlbauer"},
      {"80", "Misawa"},
      {"81", "Teichiku"},
      {"82", "Namco Ltd."},
      {"83", "LOZC"},
      {"84", "KOEI"},
      {"86", "Tokuma Shoten Intermedia"},
      {"87", "Tsukuda Original"},
      {"88", "DATAM-Polystar"},
      {"8B", "Bullet-Proof Software"},
      {"8C", "Vic Tokai Inc."},
      {"8E", "Character Soft"},
      {"8F", "I'Max"},
      {"8G", "Saurus"},
      {"8J", "General Entertainment"},
      {"8N", "Success"},
      {"8P", "Sega Japan"},
      {"90", "Takara Amusement"},
      {"91", "Chunsoft"},
      {"92", "Video System / Mc O' River"},
      {"93", "BEC"},
      {"95", "Varie"},
      {"96", "Yonezawa / S'pal"},
      {"97", "Kaneko"},
      {"99", "Marvelous Entertainment"},
      {"9A", "Nichibutsu / Nihon Bussan"},
      {"9B", "Tecmo"},
      {"9C", "Imagineer"},
      {"9F", "Nova"},
      {"9G", "Take2 / Den'Z / Global Star"},
      {"9H", "Bottom Up"},
      {"9J", "Technical Group Laboratory"},
      {"9L", "Hasbro Japan"},
      {"9N", "Marvelous Entertainment"},
      {"9P", "Keynet Inc."},
      {"9Q", "Hands-On Entertainment"},
      {"A0", "Telenet"},
      {"A1", "Hori"},
      {"A4", "Konami"},
      {"A5", "K. Amusement Leasing Co."},
      {"A6", "Kawada"},
      {"A7", "Takara"},
      {"A9", "Technos Japan Corp."},
      {"AA", "JVC / Victor"},
      {"AC", "Toei Animation"},
      {"AD", "Toho"},
      {"AF", "Namco"},
      {"AG", "Media Rings Corporation"},
      {"AH", "J-Wing"},
      {"AJ", "Pioneer LDC"},
      {"AK", "KID"},
      {"AL", "Mediafactory"},
      {"AP", "Infogrames / Hudson"},
      {"AQ", "Kiratto Ludic Inc."},
      {"B0", "Acclaim Japan"},
      {"B1", "ASCII"},
      {"B2", "Bandai"},
      {"B4", "Enix"},
      {"B6", "HAL Laboratory"},
      {"B7", "SNK"},
      {"B9", "Pony Canyon"},
      {"BA", "Culture Brain"},
      {"BB", "Sunsoft"},
      {"BC", "Toshiba EMI"},
      {"BD", "Sony Imagesoft"},
      {"BF", "Sammy"},
      {"BG", "Magical"},
      {"BH", "Visco"},
      {"BJ", "Compile"},
      {"BL", "MTO Inc."},
      {"BN", "Sunrise Interactive"},
      {"BP", "Global A Entertainment"},
      {"BQ", "Fuuki"},
      {"C0", "Taito"},
      {"C2", "Kemco"},
      {"C3", "Square"},
      {"C4", "Tokuma Shoten"},
      {"C5", "Data East"},
      {"C6", "Tonkin House / Tokyo Shoseki"},
      {"C8", "Koei"},
      {"CA", "Konami / Ultra / Palcom"},
      {"CB", "NTVIC / VAP"},
      {"CC", "Use Co., Ltd."},
      {"CD", "Meldac"},
      {"CE", "Pony Canyon / FCI "},
      {"CF", "Angel / Sotsu Agency / Sunrise"},
      {"CG", "Yumedia / Aroma Co., Ltd"},
      {"CJ", "Boss"},
      {"CK", "Axela / Crea-Tech"},
      {"CL", "Sekaibunka-Sha / Sumire Kobo / Marigul Management Inc."},
      {"CM", "Konami Computer Entertainment Osaka"},
      {"CN", "NEC Interchannel"},
      {"CP", "Enterbrain"},
      {"CQ", "From Software"},
      {"D0", "Taito / Disco"},
      {"D1", "Sofel"},
      {"D2", "Quest / Bothtec"},
      {"D3", "Sigma"},
      {"D4", "Ask Kodansha"},
      {"D6", "Naxat"},
      {"D7", "Copya System"},
      {"D8", "Capcom Co., Ltd."},
      {"D9", "Banpresto"},
      {"DA", "Tomy"},
      {"DB", "LJN Japan"},
      {"DD", "NCS"},
      {"DE", "Human Entertainment"},
      {"DF", "Altron"},
      {"DG", "Jaleco"},
      {"DH", "Gaps Inc."},
      {"DN", "Elf"},
      {"DQ", "Compile Heart"},
      {"E0", "Jaleco"},
      {"E2", "Yutaka"},
      {"E3", "Varie"},
      {"E4", "T&E Soft"},
      {"E5", "Epoch"},
      {"E7", "Athena"},
      {"E8", "Asmik"},
      {"E9", "Natsume"},
      {"EA", "King Records"},
      {"EB", "Atlus"},
      {"EC", "Epic / Sony Records"},
      {"EE", "Information Global Service"},
      {"EG", "Chatnoir"},
      {"EH", "Right Stuff"},
      {"EL", "Spike"},
      {"EM", "Konami Computer Entertainment Tokyo"},
      {"EN", "Alphadream Corporation"},
      {"EP", "Sting"},
      {"ES", "Star-Fish"},
      {"F0", "A Wave"},
      {"F1", "Motown Software"},
      {"F2", "Left Field Entertainment"},
      {"F3", "Extreme Ent. Grp."},
      {"F4", "TecMagik"},
      {"F9", "Cybersoft"},
      {"FB", "Psygnosis"},
      {"FE", "Davidson / Western Tech."},
      {"FK", "The Game Factory"},
      {"FL", "Hip Games"},
      {"FM", "Aspyr"},
      {"FP", "Mastiff"},
      {"FQ", "iQue"},
      {"FR", "Digital Tainment Pool"},
      {"FS", "XS Games / Jack Of All Games"},
      {"FT", "Daiwon"},
      {"G0", "Alpha Unit"},
      {"G1", "PCCW Japan"},
      {"G2", "Yuke's Media Creations"},
      {"G4", "KiKi Co., Ltd."},
      {"G5", "Open Sesame Inc."},
      {"G6", "Sims"},
      {"G7", "Broccoli"},
      {"G8", "Avex"},
      {"G9", "D3 Publisher"},
      {"GB", "Konami Computer Entertainment Japan"},
      {"GD", "Square-Enix"},
      {"GE", "KSG"},
      {"GF", "Micott & Basara Inc."},
      {"GH", "Orbital Media"},
      {"GJ", "Detn8 Games"},
      {"GL", "Gameloft / Ubisoft"},
      {"GM", "Gamecock Media Group"},
      {"GN", "Oxygen Games"},
      {"GT", "505 Games"},
      {"GY", "The Game Factory"},
      {"H1", "Treasure"},
      {"H2", "Aruze"},
      {"H3", "Ertain"},
      {"H4", "SNK Playmore"},
      {"HF", "Level-5"},
      {"HJ", "Genius Products"},
      {"HY", "Reef Entertainment"},
      {"HZ", "Nordcurrent"},
      {"IH", "Yojigen"},
      {"J9", "AQ Interactive"},
      {"JF", "Arc System Works"},
      {"JJ", "Deep Silver"},
      {"JW", "Atari"},
      {"K6", "Nihon System"},
      {"KB", "NIS America"},
      {"KM", "Deep Silver"},
      {"KP", "Purple Hills"},
      {"LH", "Trend Verlag / East Entertainment"},
      {"LT", "Legacy Interactive"},
      {"MJ", "Mumbo Jumbo"},
      {"MR", "Mindscape"},
      {"MS", "Milestone / UFO Interactive"},
      {"MT", "Blast!"},
      {"N9", "Terabox"},
      {"NG", "Nordic Games"},
      {"NK", "Neko Entertainment / Diffusion / Naps team"},
      {"NP", "Nobilis"},
      {"NQ", "Namco Bandai"},
      {"NR", "Data Design / Destineer Studios"},
      {"NS", "NIS America"},
      {"PL", "Playlogic"},
      {"QC", "Kadokawa Shoten"},
      {"RM", "Rondomedia"},
      {"RS", "Warner Bros. Interactive Entertainment Inc."},
      {"RT", "RTL Games"},
      {"RW", "RealNetworks"},
      {"S5", "Southpeak Interactive"},
      {"SP", "Blade Interactive Studios"},
      {"SV", "SevenGames"},
      {"SZ", "Storm City"},
      {"TK", "Tasuke / Works"},
      {"TV", "Tivola"},
      {"UG", "Metro 3D / Data Design"},
      {"VN", "Valcon Games"},
      {"VP", "Virgin Play"},
      {"VZ", "Little Orbit"},
      {"WR", "Warner Bros. Interactive Entertainment Inc."},
      {"XJ", "Xseed Games"},
      {"XS", "Aksys Games"},
      {"YT", "Valcon Games"},
      {"Z4", "Ntreev Soft"},
      {"ZA", "WBA Interactive"},
      {"ZH", "Internal Engine"},
      {"ZS", "Zinkia"},
      {"ZW", "Judo Baby"},
      {"ZX", "Topware Interactive"}};

  auto iterator = companies.find(company_id);
  if (iterator != companies.end())
    return iterator->second;
  else
    return "";
}
}
