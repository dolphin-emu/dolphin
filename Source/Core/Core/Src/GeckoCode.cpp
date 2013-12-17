// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GeckoCode.h"

#include "Thread.h"
#include "HW/Memmap.h"
#include "ConfigManager.h"

#include "vector"
#include "PowerPC/PowerPC.h"
#include "CommonPaths.h"

namespace Gecko
{
// return true if a code exists
bool GeckoCode::Exist(u32 address, u32 data)
{
	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = codes.begin(),
		codes_end = codes.end();
	for (; codes_iter != codes_end; ++codes_iter)
	{
		if (codes_iter->address == address && codes_iter->data == data)
			return true;
	}
	return false;
}

// return true if the code is identical
bool GeckoCode::Compare(GeckoCode compare) const
{
	if (codes.size() != compare.codes.size())
		return false;

	unsigned int exist = 0;
	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = codes.begin(),
		codes_end = codes.end();
	for (; codes_iter != codes_end; ++codes_iter)
	{
		if (compare.Exist(codes_iter->address, codes_iter->data))
			exist++;
	}
	return exist == codes.size();
}

static bool code_handler_installed = false;
// the currently active codes
std::vector<GeckoCode> active_codes;
static std::mutex active_codes_lock;

void SetActiveCodes(const std::vector<GeckoCode>& gcodes)
{
	std::lock_guard<std::mutex> lk(active_codes_lock);

	active_codes.clear();
	// add enabled codes
	std::vector<GeckoCode>::const_iterator
		gcodes_iter = gcodes.begin(),
		gcodes_end = gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		if (gcodes_iter->enabled)
		{
			// TODO: apply modifiers
			// TODO: don't need description or creator string, just takin up memory
			active_codes.push_back(*gcodes_iter);
		}
	}

	code_handler_installed = false;
}

bool InstallCodeHandler()
{
	u32 codelist_location = 0x800028B8; // Debugger on location (0x800022A8 = Debugger off, using codehandleronly.bin)
	std::string data;
	std::string _rCodeHandlerFilename = File::GetSysDirectory() + GECKO_CODE_HANDLER;
	if (!File::ReadFileToString(_rCodeHandlerFilename.c_str(), data))
	{
		NOTICE_LOG(ACTIONREPLAY, "Could not enable cheats because codehandler.bin was missing.");
		return false;
	}

	// Install code handler
	Memory::WriteBigEData((const u8*)data.data(), 0x80001800, data.length());

	// Turn off Pause on start
	Memory::Write_U32(0, 0x80002774);

	// Write a magic value to 'gameid' (codehandleronly does not actually read this).
	// For the purpose of this, see HLEGeckoCodehandler.
	Memory::Write_U32(0xd01f1bad, 0x80001800);

	// Create GCT in memory
	Memory::Write_U32(0x00d0c0de, codelist_location);
	Memory::Write_U32(0x00d0c0de, codelist_location + 4);

	std::lock_guard<std::mutex> lk(active_codes_lock);

	int i = 0;
	std::vector<GeckoCode>::iterator
		gcodes_iter = active_codes.begin(),
		gcodes_end = active_codes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		if (gcodes_iter->enabled)
		{
			for (auto& code : gcodes_iter->codes)
			{
				// Make sure we have enough memory to hold the code list
				if ((codelist_location + 24 + i) < 0x80003000)
				{
					Memory::Write_U32(code.address, codelist_location + 8 + i);
					Memory::Write_U32(code.data, codelist_location + 12 + i);
					i += 8;
				}
			}
		}
	}

	Memory::Write_U32(0xff000000, codelist_location + 8 + i);
	Memory::Write_U32(0x00000000, codelist_location + 12 + i);

	// Turn on codes
	Memory::Write_U8(1, 0x80001807);

	// Invalidate the icache
	for (unsigned int j = 0; j < data.length(); j += 32)
	{
		PowerPC::ppcState.iCache.Invalidate(0x80001800 + j);
	}
	return true;
}

void RunCodeHandler()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats && active_codes.size() > 0)
	{
		if (!code_handler_installed || Memory::Read_U32(0x80001800) - 0xd01f1bad > 5)
			code_handler_installed = InstallCodeHandler();

		if (!code_handler_installed)
		{
			// A warning was already issued.
			return;
		}

		if (PC == LR)
		{
			u32 oldLR = LR;
			PowerPC::CoreMode oldMode = PowerPC::GetMode();

			PC = 0x800018A8;
			LR = 0;

			// Execute the code handler in interpreter mode to track when it exits
			PowerPC::SetMode(PowerPC::MODE_INTERPRETER);

			while (PC != 0)
				PowerPC::SingleStep();

			PowerPC::SetMode(oldMode);
			PC = LR = oldLR;
		}
	}
}

}	// namespace Gecko
