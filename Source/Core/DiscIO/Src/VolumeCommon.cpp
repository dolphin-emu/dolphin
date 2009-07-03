// Copyright (C) 2003-2009 Dolphin Project.

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
		// PAL
		case 'D':
		case 'S': // <- that is shitty :) zelda demo disc
		case 'X': // XIII <- uses X but is PAL rip
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
			return IVolume::COUNTRY_USA;
			break;

		case 'J':
			return IVolume::COUNTRY_JAPAN;
			break;

		case 'K':
			return IVolume::COUNTRY_KOREA;
			break;			

		case 'O':
			return IVolume::COUNTRY_SDK;
			break;

		case 'W':
			return IVolume::COUNTRY_TAIWAN;
			break;

		default:
			WARN_LOG(DISCIO, "Unknown Country Code! %c", CountryCode);
			return IVolume::COUNTRY_UNKNOWN;
			break;
	}
}
};

