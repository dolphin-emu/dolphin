// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class CDolLoader
{
public:
	CDolLoader(const std::string& filename);
	CDolLoader(const std::vector<u8>& buffer);
	~CDolLoader();

	bool IsValid()      const { return m_is_valid; }
	bool IsWii()        const { return m_is_wii; }
	u32 GetEntryPoint() const { return m_dolheader.entryPoint; }

	// Load into emulated memory
	void Load() const;

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
	};
	SDolHeader m_dolheader;

	std::vector<std::vector<u8>> m_data_sections;
	std::vector<std::vector<u8>> m_text_sections;

	bool m_is_valid;
	bool m_is_wii;

	// Copy sections to internal buffers
	bool Initialize(const std::vector<u8>& buffer);
};
