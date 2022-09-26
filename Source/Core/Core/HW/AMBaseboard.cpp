// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <fstream>

#include "Common/Common.h" // Common
#include "Common/ChunkFile.h"
#include "Common/IOFile.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/DVD/DVDInterface.h"

#include "Core/HW/AMBaseboard.h"

namespace AMBaseboard
{

static File::IOFile		*m_netcfg;
static File::IOFile		*m_netctrl;
static File::IOFile		*m_dimm;

static u32 m_controllertype;

static unsigned char media_buffer[0x60];
static unsigned char network_command_buffer[0x200];

static inline void PrintMBBuffer( u32 Address )
{
	NOTICE_LOG_FMT(DVDINTERFACE, "GC-AM: {:08x} {:08x} {:08x} {:08x}",	Memory::Read_U32(Address),
															Memory::Read_U32(Address+4),
															Memory::Read_U32(Address+8),
															Memory::Read_U32(Address+12) );
	NOTICE_LOG_FMT(DVDINTERFACE, "GC-AM: {:08x} {:08x} {:08x} {:08x}",	Memory::Read_U32(Address+16),
															Memory::Read_U32(Address+20),
															Memory::Read_U32(Address+24),
															Memory::Read_U32(Address+28) );
}

void Init( void )
{
	//AMBaseboard::gameid = 0;
	u32 tempID = 0;
	memset( media_buffer, 0, sizeof(media_buffer) );

	//Convert game ID into hex
	// My compiler somehow throws an error instead of a warning for this sscanf,
	// Using for loop to manually convert chars to ints instead for now
	//sscanf( SConfig::GetInstance().GetGameID().c_str(), "%s", &gameid );
	std::string gameidstr = SConfig::GetInstance().GetGameID();
	for (int i = 0; i < gameidstr.length(); i++) {
		tempID += (u32) (gameidstr[i] << (8 * i));
	}
	AMBaseboard::gameid = tempID;
	//TODO: Calculate correct ID configs for each game, likely won't match
	//the original vals
	// This is checking for the real game IDs (not those people made up) (See boot.id within the game)
	switch(Common::swap32(gameid))
	{
		// SBGE - F-ZERO AX
		case 0x53424745:
			m_controllertype = 1;
			break;
		// SBLJ - VIRTUA STRIKER 4 Ver.2006
		case 0x53424C4A:
		// SBHJ - VIRTUA STRIKER 4 VER.A
		case 0x5342484A:
		// SBJJ - VIRTUA STRIKER 4
		case 0x53424A4A:
		// SBEJ - Virtua Striker 2002
		case 0x5342454A:
			m_controllertype = 2;
			break;
		// SBKJ - MARIOKART ARCADE GP
		case 0x53424B4A:
		// SBNJ - MARIOKART ARCADE GP2
		case 0x53424E4A:
			m_controllertype = 3;
			break;
		default:
		//	PanicAlertFmtT("Unknown game ID, using default controls.");
			m_controllertype = 3;
			break;
	}

	std::string netcfg_Filename( File::GetUserPath(D_TRIUSER_IDX) + "trinetcfg.bin" );
	if( File::Exists( netcfg_Filename ) )
	{
		m_netcfg = new File::IOFile( netcfg_Filename, "rb+" );
	}
	else
	{
		m_netcfg = new File::IOFile( netcfg_Filename, "wb+" );
	}

	std::string netctrl_Filename( File::GetUserPath(D_TRIUSER_IDX) +  "trinetctrl.bin" );
	if( File::Exists( netctrl_Filename ) )
	{
		m_netctrl = new File::IOFile( netctrl_Filename, "rb+" );
	}
	else
	{
		m_netctrl = new File::IOFile( netctrl_Filename, "wb+" );
	}

	std::string dimm_Filename( File::GetUserPath(D_TRIUSER_IDX) + "tridimm_" + SConfig::GetInstance().GetGameID() + ".bin" );
	if( File::Exists( dimm_Filename ) )
	{
		m_dimm = new File::IOFile( dimm_Filename, "rb+" );
	}
	else
	{
		m_dimm = new File::IOFile( dimm_Filename, "wb+" );
	}
}
u32 ExecuteCommand( u32 Command, u32 Length, u32 Address, u32 Offset )
{
	NOTICE_LOG_FMT(DVDINTERFACE, "GCAM: {:08x} {:08x} DMA=addr:{:08x},len:{:08x}",
		Command, Offset, Address, Length);

	switch(Command>>24)
	{
		// Inquiry
		case 0x12:
			return 0x21000000;
			break;
		// Read
		case 0xA8:
			if( (Offset & 0x8FFF0000) == 0x80000000 )
			{
				switch(Offset)
				{
				// Media board status (1)
				case 0x80000000:
					Memory::Write_U16( Common::swap16( 0x0100 ), Address );
					break;
				// Media board status (2)
				case 0x80000020:
					memset( Memory::GetPointer(Address), 0, Length );
					break;
				// Media board status (3)
				case 0x80000040:
					memset( Memory::GetPointer(Address), 0xFFFFFFFF, Length );
					// DIMM size (512MB)
					Memory::Write_U32( Common::swap32( 0x20000000 ), Address );
					// GCAM signature
					Memory::Write_U32( 0x4743414D, Address+4 );
					break;
				// Firmware status (1)
				case 0x80000120:
					Memory::Write_U32( Common::swap32( (u32)0x00000001 ), Address );
					break;
				// Firmware status (2)
				case 0x80000140:
					Memory::Write_U32( Common::swap32( (u32)0x00000001 ), Address );
					break;
				default:
					PrintMBBuffer(Address);
					PanicAlertFmtT("Unhandled Media Board Read");
					break;
				}
				return 0;
			}
			// Network configuration
			if( (Offset == 0x00000000) && (Length == 0x80) )
			{
				m_netcfg->Seek( 0, File::SeekOrigin::Begin );
				m_netcfg->ReadBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// DIMM memory (3MB)
			if( (Offset >= 0x1F000000) && (Offset <= 0x1F300000) )
			{
				u32 dimmoffset = Offset - 0x1F000000;
				m_dimm->Seek( dimmoffset, File::SeekOrigin::Begin );
				m_dimm->ReadBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// DIMM command
			if( (Offset >= 0x1F900000) && (Offset <= 0x1F90003F) )
			{
				u32 dimmoffset = Offset - 0x1F900000;
				memcpy( Memory::GetPointer(Address), media_buffer + dimmoffset, Length );

				NOTICE_LOG_FMT(DVDINTERFACE, "GC-AM: Read MEDIA BOARD COMM AREA ({:08x})", dimmoffset );
				PrintMBBuffer(Address);
				return 0;
			}
			// Network command
			if( (Offset >= 0x1F800200) && (Offset <= 0x1F8003FF) )
			{
				memcpy( Memory::GetPointer(Address), network_command_buffer, Length );
				return 0;
			}
			// DIMM command
			if( (Offset >= 0x84000000) && (Offset <= 0x8400005F) )
			{
				u32 dimmoffset = Offset - 0x84000000;
				memcpy( Memory::GetPointer(Address), media_buffer + dimmoffset, Length );

				NOTICE_LOG_FMT(DVDINTERFACE, "GC-AM: Read MEDIA BOARD COMM AREA ({:08x})", dimmoffset );
				PrintMBBuffer(Address);
				return 0;
			}
			// DIMM memory (8MB)
			if( (Offset >= 0xFF000000) && (Offset <= 0xFF800000) )
			{
				u32 dimmoffset = Offset - 0xFF000000;
				m_dimm->Seek( dimmoffset, File::SeekOrigin::Begin );
				m_dimm->ReadBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Network control
			if( (Offset == 0xFFFF0000) && (Length == 0x20) )
			{
				m_netctrl->Seek( 0, File::SeekOrigin::Begin );
				m_netctrl->ReadBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Max GC disc offset
			if( Offset >= 0x57058000 )
			{
				PanicAlertFmtT("Unhandled Media Board Read");
			}
			//TODO: Do we need this?  Apparently checks already in DVDInterface::CheckReadPreconditions() ?
			/*if( !DVDInterface::DVDRead( Offset, Address, Length) )
			{
				PanicAlertFmtT("Can't read from DVD_Plugin - DVD-Interface: Fatal Error");
			}*/
			break;
		// Write
		case 0xAA:
			// Network configuration
			if( (Offset == 0x00000000) && (Length == 0x80) )
			{
				m_netcfg->Seek( 0, File::SeekOrigin::Begin );
				m_netcfg->WriteBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Backup memory (8MB)
			if( (Offset >= 0x000006A0) && (Offset <= 0x00800000) )
			{
				m_dimm->Seek( Offset, File::SeekOrigin::Begin );
				m_dimm->WriteBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// DIMM memory (3MB)
			if( (Offset >= 0x1F000000) && (Offset <= 0x1F300000) )
			{
				u32 dimmoffset = Offset - 0x1F000000;
				m_dimm->Seek( dimmoffset, File::SeekOrigin::Begin );
				m_dimm->WriteBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Network command
			if( (Offset >= 0x1F800200) && (Offset <= 0x1F8003FF) )
			{
				memcpy( network_command_buffer, Memory::GetPointer(Address), Length );
				return 0;
			}
			// DIMM command
			if( (Offset >= 0x1F900000) && (Offset <= 0x1F90003F) )
			{
				u32 dimmoffset = Offset - 0x1F900000;
				memcpy( media_buffer + dimmoffset, Memory::GetPointer(Address), Length );

				ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: Write MEDIA BOARD COMM AREA ({:08x})", dimmoffset );
				PrintMBBuffer(Address);
				return 0;
			}
			// DIMM command
			if( (Offset >= 0x84000000) && (Offset <= 0x8400005F) )
			{
				u32 dimmoffset = Offset - 0x84000000;
				memcpy( media_buffer + dimmoffset, Memory::GetPointer(Address), Length );

				if( dimmoffset == 0x40 )
				{
					ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: EXECUTE ({:03X})", *(u16*)(media_buffer+0x22) );
				}

				ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: Write MEDIA BOARD COMM AREA ({:08x})", dimmoffset );
				PrintMBBuffer(Address);
				return 0;
			}
			// DIMM memory (8MB)
			if( (Offset >= 0xFF000000) && (Offset <= 0xFF800000) )
			{
				u32 dimmoffset = Offset - 0xFF000000;
				m_dimm->Seek( dimmoffset, File::SeekOrigin::Begin );
				m_dimm->WriteBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Network control
			if( (Offset == 0xFFFF0000) && (Length == 0x20) )
			{
				m_netctrl->Seek( 0, File::SeekOrigin::Begin );
				m_netctrl->WriteBytes( Memory::GetPointer(Address), Length );
				return 0;
			}
			// Max GC disc offset
			if( Offset >= 0x57058000 )
			{
				PrintMBBuffer(Address);
				PanicAlertFmtT("Unhandled Media Board Write");
			}
			break;
		// Execute
		case 0xAB:

			if( (Offset == 0) && (Length == 0) )
			{
				memset( media_buffer, 0, 0x20 );

				media_buffer[0] = media_buffer[0x20];

				// Command
				*(u16*)(media_buffer+2) = *(u16*)(media_buffer+0x22) | 0x8000;

				NOTICE_LOG_FMT(DVDINTERFACE, "GCAM: Execute command:{:03X}", *(u16*)(media_buffer+0x22) );

				switch(*(u16*)(media_buffer+0x22))
				{
					// ?
					case 0x000:
						*(u32*)(media_buffer+4) = 1;
						break;
					// DIMM size
					case 0x001:
						*(u32*)(media_buffer+4) = 0x20000000;
						break;
					// Media board status
					/*
					0x00: "Initializing media board. Please wait.."
					0x01: "Checking network. Please wait..."
					0x02: "Found a system disc. Insert a game disc"
					0x03: "Testing a game program. %d%%"
					0x04: "Loading a game program. %d%%"
					0x05: go
					0x06: error xx
					*/
					case 0x100:
						// Status
						*(u32*)(media_buffer+4) = 5;
						// Progress in %
						*(u32*)(media_buffer+8) = 100;
						break;
					// Media board version: 3.03
					case 0x101:
						// Version
						*(u16*)(media_buffer+4) = Common::swap16(0x0303);
						// Unknown
						*(u16*)(media_buffer+6) = Common::swap16(0x0100);
						*(u32*)(media_buffer+8)= 1;
						*(u32*)(media_buffer+16)= 0xFFFFFFFF;
						break;
					// System flags
					case 0x102:
						// 1: GD-ROM
						media_buffer[4] = 1;
						media_buffer[5] = 0;
						// enable development mode (Sega Boot)
						media_buffer[6] = 1;
						// Only used when inquiry 0x29
						/*
							0: NAND/MASK BOARD(HDD)
							1: NAND/MASK BOARD(MASK)
							2: NAND/MASK BOARD(NAND)
							3: NAND/MASK BOARD(NAND)
							4: DIMM BOARD (TYPE 3)
							5: DIMM BOARD (TYPE 3)
							6: DIMM BOARD (TYPE 3)
							7: N/A
							8: Unknown
						*/
						//media_buffer[7] = 0;
						break;
					// Media board serial
					case 0x103:
						memcpy(media_buffer + 4, "A89E27A50364511", 15);
						break;
					case 0x104:
						media_buffer[4] = 1;
						break;
					// Hardware test
					case 0x301:
						// Test type
						/*
							0x01: Media board
							0x04: Network
						*/
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x301: ({:08x})", *(u32*)(media_buffer+0x24) );

						//Pointer to a memory address that is directly displayed on screen as a string
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );

						// On real system it shows the status about the DIMM/GD-ROM here
						// We just show "TEST OK"
						Memory::Write_U32( 0x54534554, *(u32*)(media_buffer+0x28) );
						Memory::Write_U32( 0x004B4F20, *(u32*)(media_buffer+0x28)+4 );

						*(u32*)(media_buffer+0x04) = *(u32*)(media_buffer+0x24);
						break;
					case 0x401:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x401: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						break;
					case 0x403:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x403: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						media_buffer[4] = 1;
						break;
					case 0x404:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x404: ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						break;
					case 0x408:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x408: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						break;
					case 0x40B:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x40B: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						break;
					case 0x40C:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x40C: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x34) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x38) );
						break;
					case 0x40E:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x40E: ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x34) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x38) );
						break;
					case 0x410:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x410: ({:08x})", *(u32*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x34) );
						break;
					case 0x411:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x411: ({:08x})", *(u32*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );

						*(u32*)(media_buffer+4) = 0x46;
						break;
					// Set IP
					case 0x415:
					{
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x415: ({:08x})", *(u32*)(media_buffer+0x24) );
						// Address of string
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						// Length of string
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );

						u32 offset = *(u32*)(media_buffer+0x28) - 0x1F800200;
						NOTICE_LOG_FMT(DVDINTERFACE, "GC-AM: Set IP:{:s}", (char*)(network_command_buffer+offset) );
						break;
					}
					case 0x601:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x601");
						break;
					case 0x606:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x606: ({:04x})", *(u16*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:04x})", *(u16*)(media_buffer+0x26) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:02x})", *( u8*)(media_buffer+0x28) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:02x})", *( u8*)(media_buffer+0x29) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:04x})", *(u16*)(media_buffer+0x2A) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x2C) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x30) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x34) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x38) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x3C) );
						break;
					case 0x607:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x607: ({:04x})", *(u16*)(media_buffer+0x24) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:04x})", *(u16*)(media_buffer+0x26) );
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM:        ({:08x})", *(u32*)(media_buffer+0x28) );
						break;
					case 0x614:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: 0x601");
						break;
					default:
						ERROR_LOG_FMT(DVDINTERFACE, "GC-AM: execute buffer UNKNOWN:{:03X}", *(u16*)(media_buffer+0x22) );
						break;
					}

				memset( media_buffer + 0x20, 0, 0x20 );
				return 0x66556677;
			}

			PanicAlertFmtT("Unhandled Media Board Execute");
			break;
	}

	return 0;
}
u32 GetControllerType( void )
{
	return m_controllertype;
}
void Shutdown( void )
{
	m_netcfg->Close();
	m_netctrl->Close();
	m_dimm->Close();
}

}
