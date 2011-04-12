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

#include "TextureCacheBase.h"

TextureCacheBase* g_textureCache = NULL;

TextureCacheBase::~TextureCacheBase()
{
	for (TCacheMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
	{
		delete it->second;
	}
	m_map.clear();
}

void TextureCacheBase::Invalidate()
{
	for (TCacheMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
	{
		it->second->EvictFromTmem();
	}
}

TCacheEntryBase* TextureCacheBase::LoadEntry(u32 ramAddr,
	u32 width, u32 height, u32 levels, u32 format, u32 tlutAddr, u32 tlutFormat)
{
	TCacheMap::iterator it = m_map.find(ramAddr);
	if (it == m_map.end())
	{
		TCacheEntryBase* newEntry = CreateEntry();
		it = m_map.insert(std::make_pair(ramAddr, newEntry)).first;
	}

	// Refresh the entry if necessary (entry will check)
	it->second->Refresh(ramAddr, width, height, levels, format, tlutAddr, tlutFormat);

	return it->second;
}

void TextureCacheBase::Load(u32 tmemAddr, const u8* src, u32 size)
{
	memcpy(&m_cache[tmemAddr], src, size);
}
