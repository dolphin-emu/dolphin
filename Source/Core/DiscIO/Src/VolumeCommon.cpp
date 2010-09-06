// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#include "Volume.h"

namespace DiscIO
{
IVolume::ECountry CountrySwitch(u8 CountryCode)
{
	switch (CountryCode) 
	{
		// Region free - fall through to European defaults for now
		case 'A':

		case 'X': // Not a real region code. Used by DVDX 1.0 and
			  // The Homebrew Channel versions 1.0.4 and earlier.

		// PAL
		case 'D':
		case 'L': // Japanese import to PAL regions
		case 'M': // Japanese import to PAL regions
		case 'S': // Spanish-speaking regions
		case 'P':
			return IVolume::COUNTRY_EUROPE;
			break;
			
		case 'F':
			return IVolume::COUNTRY_FRANCE;
			break;
			
		case 'I':
			return IVolume::COUNTRY_ITALY;
			break;

		// NTSC
		case 'E':
		case 'N': // Japanese import to USA and other NTSC regions
		case 'Z': // Prince Of Persia - The Forgotten Sands (WII)
			return IVolume::COUNTRY_USA;
			break;

		case 'J':
			return IVolume::COUNTRY_JAPAN;
			break;

		case 'K':
		case 'T': // Korea with English language
		case 'Q': // Korea with Japanese language
			return IVolume::COUNTRY_KOREA;
			break;			

		case 'O':
			return IVolume::COUNTRY_SDK;
			break;

		case 'W':
			return IVolume::COUNTRY_TAIWAN;
			break;

		default:
			if (CountryCode > 'A') // Silently ignore IOS wads
				WARN_LOG(DISCIO, "Unknown Country Code! %c", CountryCode);
			return IVolume::COUNTRY_UNKNOWN;
			break;
	}
}
};

