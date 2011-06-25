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

#ifndef _TEXTURECACHEBASE_H
#define _TEXTURECACHEBASE_H

#include <map>

#include "VideoCommon.h"
#include "TextureDecoder.h"

// Common code of a texture system that:
// - Loads textures to special objects outside of main RAM (ie graphics memory)
// - Must avoid reloading textures for performance reasons

class TCacheEntryBase
{

public:

	TCacheEntryBase();
	virtual ~TCacheEntryBase() { }

	void Refresh(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
		u32 tlutAddr, u32 tlutFormat, u32 validation);

protected:

	virtual void RefreshInternal(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
		u32 tlutAddr, u32 tlutFormat, bool invalidated) = 0;

	u32 m_validation;

};

class VirtualEFBCopyBase
{

public:

	VirtualEFBCopyBase()
		: m_hash(0)
	{ }
	virtual ~VirtualEFBCopyBase() { }

	virtual void Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf) = 0;

	u64 GetHash() const { return m_hash; }
	void SetHash(u64 hash) { m_hash = hash; }

protected:
	
	// This is not maintained by VirtualEFBCopy. It must be handled externally.
	u64 m_hash;

};

// Map main RAM addresses to textures decoded from those addresses
typedef std::map<u32, TCacheEntryBase*> TCacheMap;
// Map main RAM addresses to EFB copies performed to those addresses
typedef std::map<u32, VirtualEFBCopyBase*> VirtualEFBCopyMap;

class TextureCacheBase
{

public:

	TextureCacheBase();
	virtual ~TextureCacheBase();

	// FIXME: Game can invalidate certain regions of the cache...this function
	// just invalidates the whole thing.
	void Invalidate();

	// Find the TCacheEntry, creating and refreshing if necessary
	TCacheEntryBase* LoadEntry(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat);

	void EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

protected:

	virtual TCacheEntryBase* CreateEntry() = 0;
	virtual VirtualEFBCopyBase* CreateVirtualEFBCopy() = 0;

	// Returns the size of encoded data in bytes
	virtual u32 EncodeEFBToRAM(u8* dst, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf) = 0;

	TCacheMap m_map;
	VirtualEFBCopyMap m_virtCopyMap;

	// Validation: Increments each time tmem is invalidated. Each TCacheEntry
	// has a validation number which gets compared with this.
	u32 m_validation;

};

extern TextureCacheBase* g_textureCache;

#endif
