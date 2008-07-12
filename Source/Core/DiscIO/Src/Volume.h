// Copyright (C) 2003-2008 Dolphin Project.

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
#ifndef _VOLUME_H
#define _VOLUME_H

#include <string>

#include "Common.h"

namespace DiscIO
{
class IVolume
{
	public:

		IVolume()
		{}


		virtual ~IVolume()
		{}


		virtual bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const = 0;
		virtual std::string GetName() const = 0;
		virtual std::string GetUniqueID() const = 0;


		enum ECountry
		{
			COUNTRY_EUROPE = 0,
			COUNTRY_FRANCE = 1,
			COUNTRY_USA = 2,
			COUNTRY_JAP,
			COUNTRY_UNKNOWN,
			NUMBER_OF_COUNTRIES
		};

		virtual ECountry GetCountry() const = 0;
		virtual u64 GetSize() const = 0;
};
} // namespace

#endif

