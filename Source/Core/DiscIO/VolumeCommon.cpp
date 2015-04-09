// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Volume.h"

// Increment CACHE_REVISION if the code below is modified (ISOFile.cpp & GameFile.cpp)
namespace DiscIO
{
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

std::string IVolume::GetName() const
{
	auto names = GetNames();
	if (names.empty())
		return "";
	else
		return names.cbegin()->second;
}

}
