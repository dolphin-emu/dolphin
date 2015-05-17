// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

class CDolLoader
{
public:
	CDolLoader(const std::string& filename);
	CDolLoader(u8* _pBuffer, u32 _Size);
	~CDolLoader();

	bool IsWii()        const { return m_isWii; }
	u32 GetEntryPoint() const { return m_dolheader.entryPoint; }

	// Load into emulated memory
	void Load();

private:
	enum
	{
		DOL_NUM_TEXT = 7,
		DOL_NUM_DATA = 11
	};

	struct SDolHeader
	{
		u32 textOffset[DOL_NUM_TEXT];
		u32 dataOffset[DOL_NUM_DATA];

		u32 textAddress[DOL_NUM_TEXT];
		u32 dataAddress[DOL_NUM_DATA];

		u32 textSize[DOL_NUM_TEXT];
		u32 dataSize[DOL_NUM_DATA];

		u32 bssAddress;
		u32 bssSize;
		u32 entryPoint;
		u32 padd[7];
	};
	SDolHeader m_dolheader;

	u8 *data_section[DOL_NUM_DATA];
	u8 *text_section[DOL_NUM_TEXT];

	bool m_isWii;

	// Copy sections to internal buffers
	void Initialize(u8* _pBuffer, u32 _Size);
};
