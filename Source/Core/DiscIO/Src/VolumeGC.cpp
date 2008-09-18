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

#include "stdafx.h"

#include "VolumeGC.h"
#include "StringUtil.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(IBlobReader* _pReader)
	: m_pReader(_pReader)
{}


CVolumeGC::~CVolumeGC()
{
	delete m_pReader;
}


bool
CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == NULL)
	{
		return(false);
	}

	return(m_pReader->Read(_Offset, _Length, _pBuffer));
}


std::string
CVolumeGC::GetName() const
{
	if (m_pReader == NULL)
	{
		return(false);
	}

	char Name[128];

	if (!Read(0x20, 0x60, (u8*)&Name))
	{
		return(false);
	}

	return(Name);
}


std::string
CVolumeGC::GetUniqueID() const
{
	static const std::string NO_UID("NO_UID");
	if (m_pReader == NULL)
	{
		return NO_UID;
	}

	char id[6];

	if (!Read(0, sizeof(id), reinterpret_cast<u8*>(id)))
	{
		PanicAlert("Failed to read unique ID from disc image");
		return NO_UID;
	}

	return std::string(id, sizeof(id));
}


IVolume::ECountry
CVolumeGC::GetCountry() const
{
	if (!m_pReader)
	{
		return(COUNTRY_UNKNOWN);
	}

	u8 CountryCode;
	m_pReader->Read(3, 1, &CountryCode);

	ECountry country = COUNTRY_UNKNOWN;

	switch (CountryCode)
	{
	    case 'S':
		    country = COUNTRY_EUROPE;
		    break; // PAL // <- that is shitty :) zelda demo disc

	    case 'P':
		    country = COUNTRY_EUROPE;
		    break; // PAL

	    case 'D':
		    country = COUNTRY_EUROPE;
		    break; // PAL

	    case 'F':
		    country = COUNTRY_FRANCE;
		    break; // PAL

	    case 'X':
		    country = COUNTRY_EUROPE;
		    break; // XIII <- uses X but is PAL rip

	    case 'E':
		    country = COUNTRY_USA;
		    break; // USA

	    case 'J':
		    country = COUNTRY_JAP;
		    break; // JAP

	    case 'O':
		    country = COUNTRY_UNKNOWN;
		    break; // SDK

	    default:
		    // PanicAlert(StringFromFormat("Unknown Country Code!").c_str());
		    country = COUNTRY_UNKNOWN;
		    break;
	}

	return(country);
}


u64
CVolumeGC::GetSize() const
{
	if (m_pReader)
	{
		return((size_t)m_pReader->GetDataSize());
	}
	else
	{
		return(0);
	}
}
} // namespace
