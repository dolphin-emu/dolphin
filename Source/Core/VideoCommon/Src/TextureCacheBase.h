// TextureCacheBase.h
// Nolan Check
// Pwned 4/2/2011

#ifndef _TEXTURECACHEBASE_H
#define _TEXTURECACHEBASE_H

#include "VideoCommon.h"
#include "TextureDecoder.h"

class TCacheEntryBase
{

public:

	virtual ~TCacheEntryBase() { }

	virtual void EvictFromTmem() = 0;
	virtual void Refresh(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
		u32 tlutAddr, u32 tlutFormat) = 0;

};

// Map main RAM addresses to textures decoded from those addresses
typedef std::map<u32, TCacheEntryBase*> TCacheMap;

class TextureCacheBase
{

public:

	virtual ~TextureCacheBase();

	// FIXME: Game can invalidate certain regions of the cache...this function
	// just invalidates the whole thing.
	void Invalidate();

	// Find the TCacheEntry, creating and refreshing if necessary
	TCacheEntryBase* LoadEntry(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	// Explicitly load data, bypassing cache mechanism
	void Load(u32 tmemAddr, const u8* src, u32 size);

	const u8* GetCache() const { return m_cache; }
	u8* GetCache() { return m_cache; }

	virtual void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf) = 0;

protected:

	virtual TCacheEntryBase* CreateEntry() = 0;

	TCacheMap m_map;

	// This does not hold the real cache. It only holds TLUTs and preloaded
	// textures. Cached areas are handled externally.
	// TRAM
	// STATE_TO_SAVE
	u8 m_cache[TMEM_SIZE];

};

extern TextureCacheBase* g_textureCache;

#endif
