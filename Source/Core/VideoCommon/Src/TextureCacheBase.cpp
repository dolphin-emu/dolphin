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

#include "Hash.h"
#include "HW/Memmap.h"
#include "VideoConfig.h"

TextureCacheBase* g_textureCache = NULL;

TCacheEntryBase::TCacheEntryBase()
	: m_validation(0)
{ }

void TCacheEntryBase::Refresh(u32 ramAddr, u32 width, u32 height, u32 levels, u32 format,
	u32 tlutAddr, u32 tlutFormat, u32 validation)
{
	RefreshInternal(ramAddr, width, height, levels, format, tlutAddr, tlutFormat,
		validation > m_validation);

	m_validation = validation;
}

TextureCacheBase::TextureCacheBase()
	: m_validation(1)
{ }

TextureCacheBase::~TextureCacheBase()
{
	for (VirtualEFBCopyMap::iterator it = m_virtCopyMap.begin(); it != m_virtCopyMap.end(); ++it)
	{
		delete it->second;
	}
	m_virtCopyMap.clear();

	for (TCacheMap::iterator it = m_map.begin(); it != m_map.end(); ++it)
	{
		delete it->second;
	}
	m_map.clear();
}

void TextureCacheBase::Invalidate()
{
	++m_validation;
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
	it->second->Refresh(ramAddr, width, height, levels, format, tlutAddr,
		tlutFormat, m_validation);

	return it->second;
}

void TextureCacheBase::EncodeEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	u8* dst = Memory::GetPointer(dstAddr);

	u64 encodedHash = 0;
	if (g_ActiveConfig.bEFBCopyRAMEnable)
	{
		// Take the hash of the encoded data to detect if the game overwrites
		// it.
		u32 encodeSize = EncodeEFBToRAM(dst, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
		if (encodeSize)
		{
			encodedHash = GetHash64(dst, encodeSize, encodeSize);
			DEBUG_LOG(VIDEO, "Hash of EFB copy at 0x%.08X was taken... 0x%.016llX", dstAddr, encodedHash);
		}
	}
	else if (g_ActiveConfig.bEFBCopyVirtualEnable)
	{
		static u64 canaryEgg = 0x79706F4342464500; // '\0EFBCopy'

		// We aren't encoding to RAM but we are making a TCL, so put a piece of
		// canary data in RAM to detect if the game overwrites it.
		encodedHash = canaryEgg;
		++canaryEgg;

		// There will be at least 32 bytes that are safe to write here.
		*(u64*)dst = encodedHash;
	}

	if (g_ActiveConfig.bEFBCopyVirtualEnable)
	{
		VirtualEFBCopyBase* virt = NULL;

		VirtualEFBCopyMap::iterator virtIt = m_virtCopyMap.find(dstAddr);
		if (virtIt == m_virtCopyMap.end())
		{
			virt = CreateVirtualEFBCopy();
			virtIt = m_virtCopyMap.insert(std::make_pair(dstAddr, virt)).first;
		}
		else
			virt = virtIt->second;

		virt->Update(dstAddr, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf);
		virt->SetHash(encodedHash);
	}
}
