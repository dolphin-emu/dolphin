// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{

static const unsigned int WII_BANNER_WIDTH = 192;
static const unsigned int WII_BANNER_HEIGHT = 64;
static const unsigned int WII_BANNER_SIZE = WII_BANNER_WIDTH * WII_BANNER_HEIGHT * 2;
static const unsigned int WII_BANNER_OFFSET = 0xA0;

std::vector<u32> IVolume::GetBanner(int* width, int* height) const
{
	*width = 0;
	*height = 0;

	u64 title_id = 0;
	GetTitleID(&title_id);

	std::string file_name = StringFromFormat("%s/title/%08x/%08x/data/banner.bin",
		File::GetUserPath(D_WIIROOT_IDX).c_str(), (u32)(title_id >> 32), (u32)title_id);
	if (!File::Exists(file_name))
		return std::vector<u32>();

	if (File::GetSize(file_name) < WII_BANNER_OFFSET + WII_BANNER_SIZE)
		return std::vector<u32>();

	File::IOFile file(file_name, "rb");
	if (!file.Seek(WII_BANNER_OFFSET, SEEK_SET))
		return std::vector<u32>();

	std::vector<u8> banner_file(WII_BANNER_SIZE);
	if (!file.ReadBytes(banner_file.data(), banner_file.size()))
		return std::vector<u32>();

	std::vector<u32> image_buffer(WII_BANNER_WIDTH * WII_BANNER_HEIGHT);
	ColorUtil::decode5A3image(image_buffer.data(), (u16*)banner_file.data(), WII_BANNER_WIDTH, WII_BANNER_HEIGHT);

	*width = WII_BANNER_WIDTH;
	*height = WII_BANNER_HEIGHT;
	return image_buffer;
}

std::map<IVolume::ELanguage, std::string> IVolume::ReadWiiNames(const std::vector<u8>& data)
{
	std::map<IVolume::ELanguage, std::string> names;
	for (size_t i = 0; i < NUMBER_OF_LANGUAGES; ++i)
	{
		size_t name_start = NAME_BYTES_LENGTH * i;
		size_t name_end = name_start + NAME_BYTES_LENGTH;
		if (data.size() >= name_end)
		{
			u16* temp = (u16*)(data.data() + name_start);
			std::wstring out_temp(NAME_STRING_LENGTH, '\0');
			std::transform(temp, temp + out_temp.size(), out_temp.begin(), (u16(&)(u16))Common::swap16);
			out_temp.erase(std::find(out_temp.begin(), out_temp.end(), 0x00), out_temp.end());
			std::string name = UTF16ToUTF8(out_temp);
			if (!name.empty())
				names[(IVolume::ELanguage)i] = name;
		}
	}
	return names;
}

// Increment CACHE_REVISION if the code below is modified (ISOFile.cpp & GameFile.cpp)
IVolume::ECountry CountrySwitch(u8 country_code)
{
	switch (country_code)
	{
		// Region free - Uses European flag as placeholder
		case 'A':
			return IVolume::COUNTRY_WORLD;

		// PAL
		case 'D':
			return IVolume::COUNTRY_GERMANY;

		case 'X': // Used by a couple PAL games
		case 'Y': // German, French
		case 'L': // Japanese import to PAL regions
		case 'M': // Japanese import to PAL regions
		case 'P':
			return IVolume::COUNTRY_EUROPE;

		case 'U':
			return IVolume::COUNTRY_AUSTRALIA;

		case 'F':
			return IVolume::COUNTRY_FRANCE;

		case 'I':
			return IVolume::COUNTRY_ITALY;

		case 'H':
			return IVolume::COUNTRY_NETHERLANDS;

		case 'R':
			return IVolume::COUNTRY_RUSSIA;

		case 'S':
			return IVolume::COUNTRY_SPAIN;

		// NTSC
		case 'E':
		case 'N': // Japanese import to USA and other NTSC regions
		case 'Z': // Prince of Persia - The Forgotten Sands (Wii)
		case 'B': // Ufouria: The Saga (Virtual Console)
			return IVolume::COUNTRY_USA;

		case 'J':
			return IVolume::COUNTRY_JAPAN;

		case 'K':
		case 'Q': // Korea with Japanese language
		case 'T': // Korea with English language
			return IVolume::COUNTRY_KOREA;

		case 'W':
			return IVolume::COUNTRY_TAIWAN;

		default:
			if (country_code > 'A') // Silently ignore IOS wads
				WARN_LOG(DISCIO, "Unknown Country Code! %c", country_code);
			return IVolume::COUNTRY_UNKNOWN;
	}
}

u8 GetSysMenuRegion(u16 _TitleVersion)
{
	switch (_TitleVersion)
	{
	case 128: case 192: case 224: case 256:
	case 288: case 352: case 384: case 416:
	case 448: case 480: case 512:
		return 'J';
	case 97:  case 193: case 225: case 257:
	case 289: case 353: case 385: case 417:
	case 449: case 481: case 513:
		return 'E';
	case 130: case 162: case 194: case 226:
	case 258: case 290: case 354: case 386:
	case 418: case 450: case 482: case 514:
		return 'P';
	case 326: case 390: case 454: case 486:
	case 518:
		return 'K';
	default:
		return 'A';
	}
}

std::string GetCompanyFromID(const std::string& company_id)
{
	static const std::map<std::string, std::string> companies =
	{
		{ "01", "Nintendo" },
		{ "02", "Rocket Games / Ajinomoto" },
		{ "03", "Imagineer-Zoom" },
		{ "04", "Gray Matter" },
		{ "05", "Zamuse" },
		{ "06", "Falcom" },
		{ "07", "Enix" },
		{ "08", "Capcom" },
		{ "09", "Hot B Co." },
		{ "0A", "Jaleco" },
		{ "0B", "Coconuts Japan" },
		{ "0C", "Coconuts Japan / GX Media" },
		{ "0D", "Micronet" },
		{ "0E", "Technos" },
		{ "0F", "Mebio Software" },
		{ "0G", "Shouei System" },
		{ "0H", "Starfish" },
		{ "0J", "Mitsui Fudosan / Dentsu" },
		{ "0L", "Warashi Inc." },
		{ "0N", "Nowpro" },
		{ "0P", "Game Village" },
		{ "0Q", "IE Institute" },
		{ "12", "Infocom" },
		{ "13", "Electronic Arts Japan" },
		{ "15", "Cobra Team" },
		{ "16", "Human / Field" },
		{ "17", "KOEI" },
		{ "18", "Hudson Soft" },
		{ "19", "S.C.P." },
		{ "1A", "Yanoman" },
		{ "1C", "Tecmo Products" },
		{ "1D", "Japan Glary Business" },
		{ "1E", "Forum / OpenSystem" },
		{ "1F", "Virgin Games (Japan)" },
		{ "1G", "SMDE" },
		{ "1J", "Daikokudenki" },
		{ "1P", "Creatures Inc." },
		{ "1Q", "TDK Deep Impresion" },
		{ "20", "Zoo" },
		{ "21", "Sunsoft / Tokai Engineering" },
		{ "22", "Planning Office Wada / VR1 Japan" },
		{ "23", "Micro World" },
		{ "25", "San-X" },
		{ "26", "Enix" },
		{ "27", "Loriciel / Electro Brain" },
		{ "28", "Kemco Japan" },
		{ "29", "Seta" },
		{ "2A", "Culture Brain" },
		{ "2C", "Palsoft" },
		{ "2D", "Visit Co., Ltd." },
		{ "2E", "Intec" },
		{ "2F", "System Sacom" },
		{ "2G", "Poppo" },
		{ "2H", "Ubisoft Japan" },
		{ "2J", "Media Works" },
		{ "2K", "NEC InterChannel" },
		{ "2L", "Tam" },
		{ "2M", "Jordan" },
		{ "2N", "Smilesoft / Rocket" },
		{ "2Q", "Mediakite" },
		{ "30", "Viacom" },
		{ "31", "Carrozzeria" },
		{ "32", "Dynamic" },
		{ "34", "Magifact" },
		{ "35", "Hect" },
		{ "36", "Codemasters" },
		{ "37", "Taito / GAGA Communications" },
		{ "38", "Laguna" },
		{ "39", "Telstar / Event / Taito " },
		{ "3B", "Arcade Zone Ltd." },
		{ "3C", "Entertainment International / Empire Software" },
		{ "3D", "Loriciel" },
		{ "3E", "Gremlin Graphics" },
		{ "3F", "K. Amusement Leasing Co." },
		{ "40", "Seika Corp." },
		{ "41", "Ubisoft Entertainment" },
		{ "42", "Sunsoft US" },
		{ "44", "Life Fitness" },
		{ "46", "System 3" },
		{ "47", "Spectrum Holobyte" },
		{ "49", "IREM" },
		{ "4B", "Raya Systems" },
		{ "4C", "Renovation Products" },
		{ "4D", "Malibu Games" },
		{ "4F", "Eidos" },
		{ "4G", "Playmates Interactive" },
		{ "4J", "Fox Interactive" },
		{ "4K", "Time Warner Interactive" },
		{ "4Q", "Disney Interactive" },
		{ "4S", "Black Pearl" },
		{ "4U", "Advanced Productions" },
		{ "4X", "GT Interactive" },
		{ "4Y", "Rare" },
		{ "4Z", "Crave Entertainment" },
		{ "50", "Absolute Entertainment" },
		{ "51", "Acclaim" },
		{ "52", "Activision" },
		{ "53", "American Sammy" },
		{ "54", "Take 2 Interactive / GameTek" },
		{ "55", "Hi Tech" },
		{ "56", "LJN Ltd." },
		{ "58", "Mattel" },
		{ "5A", "Mindscape / Red Orb Entertainment" },
		{ "5B", "Romstar" },
		{ "5C", "Taxan" },
		{ "5D", "Midway / Tradewest" },
		{ "5F", "American Softworks" },
		{ "5G", "Majesco Sales Inc." },
		{ "5H", "3DO" },
		{ "5K", "Hasbro" },
		{ "5L", "NewKidCo" },
		{ "5M", "Telegames" },
		{ "5N", "Metro3D" },
		{ "5P", "Vatical Entertainment" },
		{ "5Q", "LEGO Media" },
		{ "5S", "Xicat Interactive" },
		{ "5T", "Cryo Interactive" },
		{ "5W", "Red Storm Entertainment" },
		{ "5X", "Microids" },
		{ "5Z", "Data Design / Conspiracy / Swing" },
		{ "60", "Titus" },
		{ "61", "Virgin Interactive" },
		{ "62", "Maxis" },
		{ "64", "LucasArts Entertainment" },
		{ "67", "Ocean" },
		{ "68", "Bethesda Softworks" },
		{ "69", "Electronic Arts" },
		{ "6B", "Laser Beam" },
		{ "6E", "Elite Systems" },
		{ "6F", "Electro Brain" },
		{ "6G", "The Learning Company" },
		{ "6H", "BBC" },
		{ "6J", "Software 2000" },
		{ "6K", "UFO Interactive Games" },
		{ "6L", "BAM! Entertainment" },
		{ "6M", "Studio 3" },
		{ "6Q", "Classified Games" },
		{ "6S", "TDK Mediactive" },
		{ "6U", "DreamCatcher" },
		{ "6V", "JoWood Produtions" },
		{ "6W", "Sega" },
		{ "6X", "Wannado Edition" },
		{ "6Y", "LSP (Light & Shadow Prod.)" },
		{ "6Z", "ITE Media" },
		{ "70", "Atari (Infogrames)" },
		{ "71", "Interplay" },
		{ "72", "JVC (US)" },
		{ "73", "Parker Brothers" },
		{ "75", "Sales Curve (Storm / SCI)" },
		{ "78", "THQ" },
		{ "79", "Accolade" },
		{ "7A", "Triffix Entertainment" },
		{ "7C", "Microprose Software" },
		{ "7D", "Sierra / Universal Interactive" },
		{ "7F", "Kemco" },
		{ "7G", "Rage Software" },
		{ "7H", "Encore" },
		{ "7J", "Zoo" },
		{ "7K", "Kiddinx" },
		{ "7L", "Simon & Schuster Interactive" },
		{ "7M", "Asmik Ace Entertainment Inc." },
		{ "7N", "Empire Interactive" },
		{ "7Q", "Jester Interactive" },
		{ "7S", "Rockstar Games" },
		{ "7T", "Scholastic" },
		{ "7U", "Ignition Entertainment" },
		{ "7V", "Summitsoft" },
		{ "7W", "Stadlbauer" },
		{ "80", "Misawa" },
		{ "81", "Teichiku" },
		{ "82", "Namco Ltd." },
		{ "83", "LOZC" },
		{ "84", "KOEI" },
		{ "86", "Tokuma Shoten Intermedia" },
		{ "87", "Tsukuda Original" },
		{ "88", "DATAM-Polystar" },
		{ "8B", "Bullet-Proof Software" },
		{ "8C", "Vic Tokai Inc." },
		{ "8E", "Character Soft" },
		{ "8F", "I'Max" },
		{ "8G", "Saurus" },
		{ "8J", "General Entertainment" },
		{ "8N", "Success" },
		{ "8P", "Sega Japan" },
		{ "90", "Takara Amusement" },
		{ "91", "Chunsoft" },
		{ "92", "Video System / Mc O' River" },
		{ "93", "BEC" },
		{ "95", "Varie" },
		{ "96", "Yonezawa / S'pal" },
		{ "97", "Kaneko" },
		{ "99", "Marvelous Entertainment" },
		{ "9A", "Nichibutsu / Nihon Bussan" },
		{ "9B", "Tecmo" },
		{ "9C", "Imagineer" },
		{ "9F", "Nova" },
		{ "9G", "Take2 / Den'Z / Global Star" },
		{ "9H", "Bottom Up" },
		{ "9J", "Technical Group Laboratory" },
		{ "9L", "Hasbro Japan" },
		{ "9N", "Marvelous Entertainment" },
		{ "9P", "Keynet Inc." },
		{ "9Q", "Hands-On Entertainment" },
		{ "A0", "Telenet" },
		{ "A1", "Hori" },
		{ "A4", "Konami" },
		{ "A5", "K. Amusement Leasing Co." },
		{ "A6", "Kawada" },
		{ "A7", "Takara" },
		{ "A9", "Technos Japan Corp." },
		{ "AA", "JVC / Victor" },
		{ "AC", "Toei Animation" },
		{ "AD", "Toho" },
		{ "AF", "Namco" },
		{ "AG", "Media Rings Corporation" },
		{ "AH", "J-Wing" },
		{ "AJ", "Pioneer LDC" },
		{ "AK", "KID" },
		{ "AL", "Mediafactory" },
		{ "AP", "Infogrames / Hudson" },
		{ "AQ", "Kiratto Ludic Inc." },
		{ "B0", "Acclaim Japan" },
		{ "B1", "ASCII" },
		{ "B2", "Bandai" },
		{ "B4", "Enix" },
		{ "B6", "HAL Laboratory" },
		{ "B7", "SNK" },
		{ "B9", "Pony Canyon" },
		{ "BA", "Culture Brain" },
		{ "BB", "Sunsoft" },
		{ "BC", "Toshiba EMI" },
		{ "BD", "Sony Imagesoft" },
		{ "BF", "Sammy" },
		{ "BG", "Magical" },
		{ "BH", "Visco" },
		{ "BJ", "Compile" },
		{ "BL", "MTO Inc." },
		{ "BN", "Sunrise Interactive" },
		{ "BP", "Global A Entertainment" },
		{ "BQ", "Fuuki" },
		{ "C0", "Taito" },
		{ "C2", "Kemco" },
		{ "C3", "Square" },
		{ "C4", "Tokuma Shoten" },
		{ "C5", "Data East" },
		{ "C6", "Tonkin House / Tokyo Shoseki" },
		{ "C8", "Koei" },
		{ "CA", "Konami / Ultra / Palcom" },
		{ "CB", "NTVIC / VAP" },
		{ "CC", "Use Co., Ltd." },
		{ "CD", "Meldac" },
		{ "CE", "Pony Canyon / FCI " },
		{ "CF", "Angel / Sotsu Agency / Sunrise" },
		{ "CG", "Yumedia / Aroma Co., Ltd" },
		{ "CJ", "Boss" },
		{ "CK", "Axela / Crea-Tech" },
		{ "CL", "Sekaibunka-Sha / Sumire Kobo / Marigul Management Inc." },
		{ "CM", "Konami Computer Entertainment Osaka" },
		{ "CN", "NEC Interchannel" },
		{ "CP", "Enterbrain" },
		{ "CQ", "From Software" },
		{ "D0", "Taito / Disco" },
		{ "D1", "Sofel" },
		{ "D2", "Quest / Bothtec" },
		{ "D3", "Sigma" },
		{ "D4", "Ask Kodansha" },
		{ "D6", "Naxat" },
		{ "D7", "Copya System" },
		{ "D8", "Capcom Co., Ltd." },
		{ "D9", "Banpresto" },
		{ "DA", "Tomy" },
		{ "DB", "LJN Japan" },
		{ "DD", "NCS" },
		{ "DE", "Human Entertainment" },
		{ "DF", "Altron" },
		{ "DG", "Jaleco" },
		{ "DH", "Gaps Inc." },
		{ "DN", "Elf" },
		{ "DQ", "Compile Heart" },
		{ "E0", "Jaleco" },
		{ "E2", "Yutaka" },
		{ "E3", "Varie" },
		{ "E4", "T&E Soft" },
		{ "E5", "Epoch" },
		{ "E7", "Athena" },
		{ "E8", "Asmik" },
		{ "E9", "Natsume" },
		{ "EA", "King Records" },
		{ "EB", "Atlus" },
		{ "EC", "Epic / Sony Records" },
		{ "EE", "Information Global Service" },
		{ "EG", "Chatnoir" },
		{ "EH", "Right Stuff" },
		{ "EL", "Spike" },
		{ "EM", "Konami Computer Entertainment Tokyo" },
		{ "EN", "Alphadream Corporation" },
		{ "EP", "Sting" },
		{ "ES", "Star-Fish" },
		{ "F0", "A Wave" },
		{ "F1", "Motown Software" },
		{ "F2", "Left Field Entertainment" },
		{ "F3", "Extreme Ent. Grp." },
		{ "F4", "TecMagik" },
		{ "F9", "Cybersoft" },
		{ "FB", "Psygnosis" },
		{ "FE", "Davidson / Western Tech." },
		{ "FK", "The Game Factory" },
		{ "FL", "Hip Games" },
		{ "FM", "Aspyr" },
		{ "FP", "Mastiff" },
		{ "FQ", "iQue" },
		{ "FR", "Digital Tainment Pool" },
		{ "FS", "XS Games / Jack Of All Games" },
		{ "FT", "Daiwon" },
		{ "G0", "Alpha Unit" },
		{ "G1", "PCCW Japan" },
		{ "G2", "Yuke's Media Creations" },
		{ "G4", "KiKi Co., Ltd." },
		{ "G5", "Open Sesame Inc." },
		{ "G6", "Sims" },
		{ "G7", "Broccoli" },
		{ "G8", "Avex" },
		{ "G9", "D3 Publisher" },
		{ "GB", "Konami Computer Entertainment Japan" },
		{ "GD", "Square-Enix" },
		{ "GE", "KSG" },
		{ "GF", "Micott & Basara Inc." },
		{ "GH", "Orbital Media" },
		{ "GJ", "Detn8 Games" },
		{ "GL", "Gameloft / Ubisoft" },
		{ "GM", "Gamecock Media Group" },
		{ "GN", "Oxygen Games" },
		{ "GT", "505 Games" },
		{ "GY", "The Game Factory" },
		{ "H1", "Treasure" },
		{ "H2", "Aruze" },
		{ "H3", "Ertain" },
		{ "H4", "SNK Playmore" },
		{ "HF", "Level-5" },
		{ "HJ", "Genius Products" },
		{ "HY", "Reef Entertainment" },
		{ "HZ", "Nordcurrent" },
		{ "IH", "Yojigen" },
		{ "J9", "AQ Interactive" },
		{ "JF", "Arc System Works" },
		{ "JJ", "Deep Silver" },
		{ "JW", "Atari" },
		{ "K6", "Nihon System" },
		{ "KB", "NIS America" },
		{ "KM", "Deep Silver" },
		{ "KP", "Purple Hills" },
		{ "LH", "Trend Verlag / East Entertainment" },
		{ "LT", "Legacy Interactive" },
		{ "MJ", "Mumbo Jumbo" },
		{ "MR", "Mindscape" },
		{ "MS", "Milestone / UFO Interactive" },
		{ "MT", "Blast!" },
		{ "N9", "Terabox" },
		{ "NG", "Nordic Games" },
		{ "NK", "Neko Entertainment / Diffusion / Naps team" },
		{ "NP", "Nobilis" },
		{ "NQ", "Namco Bandai" },
		{ "NR", "Data Design / Destineer Studios" },
		{ "PL", "Playlogic" },
		{ "RM", "Rondomedia" },
		{ "RS", "Warner Bros. Interactive Entertainment Inc." },
		{ "RT", "RTL Games" },
		{ "RW", "RealNetworks" },
		{ "S5", "Southpeak Interactive" },
		{ "SP", "Blade Interactive Studios" },
		{ "SV", "SevenGames" },
		{ "SZ", "Storm City" },
		{ "TK", "Tasuke / Works" },
		{ "TV", "Tivola" },
		{ "UG", "Metro 3D / Data Design" },
		{ "VN", "Valcon Games" },
		{ "VP", "Virgin Play" },
		{ "VZ", "Little Orbit" },
		{ "WR", "Warner Bros. Interactive Entertainment Inc." },
		{ "XJ", "Xseed Games" },
		{ "XS", "Aksys Games" },
		{ "YT", "Valcon Games" },
		{ "Z4", "Ntreev Soft" },
		{ "ZA", "WBA Interactive" },
		{ "ZH", "Internal Engine" },
		{ "ZS", "Zinkia" },
		{ "ZW", "Judo Baby" },
		{ "ZX", "Topware Interactive" }
	};

	auto iterator = companies.find(company_id);
	if (iterator != companies.end())
		return iterator->second;
	else
		return "";
}

}
