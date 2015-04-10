// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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

	u64 TitleID = 0;
	GetTitleID((u8*)&TitleID);
	TitleID = Common::swap64(TitleID);

	std::string file_name = StringFromFormat("%stitle/%08x/%08x/data/banner.bin",
		File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(TitleID >> 32), (u32)TitleID);
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

std::map<IVolume::ELanguage, std::string> IVolume::ReadWiiNames(std::vector<u8>& data)
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

}
