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

#ifndef __ISOFILE_H_
#define __ISOFILE_H_

#include "Volume.h"

class GameListItem
{
public:
	GameListItem(const std::string& _rFileName);
	~GameListItem();

	bool IsValid() const {return m_Valid;}
	const std::string& GetFileName() const {return m_FileName;}
	const std::string& GetName() const {return m_Name;}
	const std::string& GetCompany() const {return m_Company;}
	const std::string& GetDescription() const {return m_Description;}
	const std::string& GetUniqueID() const {return m_UniqueID;}
	DiscIO::IVolume::ECountry GetCountry() const {return m_Country;}
	bool IsCompressed() const {return m_BlobCompressed;}
	u64 GetFileSize() const {return m_FileSize;}
	u64 GetVolumeSize() const {return m_VolumeSize;}
	#ifdef USE_WX
	const wxImage& GetImage() const {return m_Image;}
	#endif

private:
	std::string m_FileName;
	std::string m_Name;
	std::string m_Company;
	std::string m_Description;
	std::string m_UniqueID;

	u64 m_FileSize;
	u64 m_VolumeSize;

	DiscIO::IVolume::ECountry m_Country;

	#ifdef USE_WX
	wxImage m_Image;
	#endif

	bool m_Valid;

	bool m_BlobCompressed;
};


#endif
