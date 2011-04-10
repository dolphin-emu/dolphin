// TextureCacheBase.cpp
// Nolan Check
// Pwned 4/2/2011

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
