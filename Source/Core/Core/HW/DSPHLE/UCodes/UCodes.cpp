// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/Hash.h"
#include "Common/StringUtil.h"

#include "Core/HW/DSPHLE/UCodes/AX.h"
#include "Core/HW/DSPHLE/UCodes/AXWii.h"
#include "Core/HW/DSPHLE/UCodes/CARD.h"
#include "Core/HW/DSPHLE/UCodes/GBA.h"
#include "Core/HW/DSPHLE/UCodes/INIT.h"
#include "Core/HW/DSPHLE/UCodes/ROM.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"

UCodeInterface* UCodeFactory(u32 crc, DSPHLE *dsphle, bool wii)
{
	switch (crc)
	{
	case UCODE_ROM:
		INFO_LOG(DSPHLE, "Switching to ROM ucode");
		return new ROMUCode(dsphle, crc);

	case UCODE_INIT_AUDIO_SYSTEM:
		INFO_LOG(DSPHLE, "Switching to INIT ucode");
		return new INITUCode(dsphle, crc);

	case 0x65d6cc6f: // CARD
		INFO_LOG(DSPHLE, "Switching to CARD ucode");
		return new CARDUCode(dsphle, crc);

	case 0xdd7e72d5:
		INFO_LOG(DSPHLE, "Switching to GBA ucode");
		return new GBAUCode(dsphle, crc);

	case 0x3ad3b7ac: // Naruto 3, Paper Mario - The Thousand Year Door
	case 0x3daf59b9: // Alien Hominid
	case 0x4e8a8b21: // spdemo, Crazy Taxi, 18 Wheeler, Disney, Monkeyball 1/2, Cubivore, Nintendo Puzzle Collection, Wario,
	                 // Capcom vs. SNK 2, Naruto 2, Lost Kingdoms, Star Fox, Mario Party 4, Mortal Kombat,
	                 // Smugglers Run Warzone, Smash Brothers, Sonic Mega Collection, ZooCube
	                 // nddemo, Star Fox
	case 0x07f88145: // bustamove, Ikaruga, F-Zero GX, Robotech Battle Cry, Star Soldier, Soul Calibur 2,
		// Zelda:OOT, Tony Hawk, Viewtiful Joe
	case 0xe2136399: // Billy Hatcher, Dragon Ball Z, Mario Party 5, TMNT, 1080° Avalanche
	case 0x3389a79e: // MP1/MP2 Wii (Metroid Prime Trilogy)
		INFO_LOG(DSPHLE, "CRC %08x: AX ucode chosen", crc);
		return new AXUCode(dsphle, crc);

	case 0x6ba3b3ea: // IPL - PAL
	case 0x24b22038: // IPL - NTSC/NTSC-JAP
	case 0x42f64ac4: // Luigi's Mansion
	case 0x4be6a5cb: // AC, Pikmin
		INFO_LOG(DSPHLE, "CRC %08x: JAC (early Zelda) ucode chosen", crc);
		return new ZeldaUCode(dsphle, crc);

	case 0x6CA33A6D: // DK Jungle Beat
	case 0x86840740: // Zelda WW - US
	case 0x56d36052: // Mario Sunshine
	case 0x2fcdf1ec: // Mario Kart, Zelda 4 Swords
	case 0x267fd05a: // Pikmin PAL
		INFO_LOG(DSPHLE, "CRC %08x: Zelda ucode chosen", crc);
		return new ZeldaUCode(dsphle, crc);

	// WII CRCs
	case 0xb7eb9a9c: // Wii Pikmin - PAL
	case 0xeaeb38cc: // Wii Pikmin 2 - PAL
	case 0x6c3f6f94: // Zelda TP - PAL
	case 0xd643001f: // Mario Galaxy - PAL / WII DK Jungle Beat - PAL
		INFO_LOG(DSPHLE, "CRC %08x: Zelda Wii ucode chosen\n", crc);
		return new ZeldaUCode(dsphle, crc);

	case 0x2ea36ce6: // Some Wii demos
	case 0x5ef56da3: // AX demo
	case 0x347112ba: // Raving Rabbids
	case 0xfa450138: // Wii Sports - PAL
	case 0xadbc06bd: // Elebits
	case 0x4cc52064: // Bleach: Versus Crusade
	case 0xd9c4bf34: // WiiMenu
		INFO_LOG(DSPHLE, "CRC %08x: Wii - AXWii chosen", crc);
		return new AXWiiUCode(dsphle, crc);

	default:
		if (wii)
		{
			PanicAlert("DSPHLE: Unknown ucode (CRC = %08x) - forcing AXWii.\n\nTry LLE emulator if this is homebrew.", crc);
			return new AXWiiUCode(dsphle, crc);
		}
		else
		{
			PanicAlert("DSPHLE: Unknown ucode (CRC = %08x) - forcing AX.\n\nTry LLE emulator if this is homebrew.", crc);
			return new AXUCode(dsphle, crc);
		}

	case UCODE_NULL:
		break;
	}

	return nullptr;
}

bool UCodeInterface::NeedsResumeMail()
{
	if (m_needs_resume_mail)
	{
		m_needs_resume_mail = false;
		return true;
	}
	return false;
}

void UCodeInterface::PrepareBootUCode(u32 mail)
{
	switch (m_next_ucode_steps)
	{
	case 0: m_next_ucode.mram_dest_addr = mail;          break;
	case 1: m_next_ucode.mram_size      = mail & 0xffff; break;
	case 2: m_next_ucode.mram_dram_addr = mail & 0xffff; break;
	case 3: m_next_ucode.iram_mram_addr = mail;          break;
	case 4: m_next_ucode.iram_size      = mail & 0xffff; break;
	case 5: m_next_ucode.iram_dest      = mail & 0xffff; break;
	case 6: m_next_ucode.iram_startpc   = mail & 0xffff; break;
	case 7: m_next_ucode.dram_mram_addr = mail;          break;
	case 8: m_next_ucode.dram_size      = mail & 0xffff; break;
	case 9: m_next_ucode.dram_dest      = mail & 0xffff; break;
	}
	m_next_ucode_steps++;

	if (m_next_ucode_steps == 10)
	{
		m_next_ucode_steps = 0;
		m_needs_resume_mail = true;
		m_upload_setup_in_progress = false;

		u32 ector_crc = HashEctor(
			(u8*)HLEMemory_Get_Pointer(m_next_ucode.iram_mram_addr),
			m_next_ucode.iram_size);

#if defined(_DEBUG) || defined(DEBUGFAST)
		std::string ucode_dump_path = StringFromFormat(
			"%sDSP_UC_%08X.bin", File::GetUserPath(D_DUMPDSP_IDX).c_str(), ector_crc);

		File::IOFile fp(ucode_dump_path, "wb");
		if (fp)
		{
			fp.WriteArray((u8*)Memory::GetPointer(m_next_ucode.iram_mram_addr),
			              m_next_ucode.iram_size);
		}
#endif

		DEBUG_LOG(DSPHLE, "PrepareBootUCode 0x%08x", ector_crc);
		DEBUG_LOG(DSPHLE, "DRAM -> MRAM: src %04x dst %08x size %04x",
			m_next_ucode.mram_dram_addr, m_next_ucode.mram_dest_addr,
			m_next_ucode.mram_size);
		DEBUG_LOG(DSPHLE, "MRAM -> IRAM: src %08x dst %04x size %04x startpc %04x",
			m_next_ucode.iram_mram_addr, m_next_ucode.iram_dest,
			m_next_ucode.iram_size, m_next_ucode.iram_startpc);
		DEBUG_LOG(DSPHLE, "MRAM -> DRAM: src %08x dst %04x size %04x",
			m_next_ucode.dram_mram_addr, m_next_ucode.dram_dest,
			m_next_ucode.dram_size);

		if (m_next_ucode.mram_size)
		{
			WARN_LOG(DSPHLE,
				"Trying to boot new ucode with dram download - not implemented");
		}
		if (m_next_ucode.dram_size)
		{
			WARN_LOG(DSPHLE,
				"Trying to boot new ucode with dram upload - not implemented");
		}

		m_dsphle->SwapUCode(ector_crc);
	}
}

void UCodeInterface::DoStateShared(PointerWrap &p)
{
	p.Do(m_upload_setup_in_progress);
	p.Do(m_next_ucode);
	p.Do(m_next_ucode_steps);
	p.Do(m_needs_resume_mail);
}
