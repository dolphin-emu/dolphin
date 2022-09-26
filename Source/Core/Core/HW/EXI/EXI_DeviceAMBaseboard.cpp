// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "Common/CommonTypes.h"
#include "Common/ChunkFile.h"

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/IOFile.h"
#include "Common/FileUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceAMBaseboard.h"

CEXIAMBaseboard::CEXIAMBaseboard()
	: m_position(0)
	, m_have_irq(false)
{
	std::string backup_Filename( File::GetUserPath(D_TRIUSER_IDX) + "tribackup_" + SConfig::GetInstance().GetGameID() + ".bin" );

	if( File::Exists( backup_Filename ) )
	{
		m_backup = new File::IOFile( backup_Filename, "rb+" );
	}
	else
	{
		m_backup = new File::IOFile( backup_Filename, "wb+" );
	}
}

void CEXIAMBaseboard::SetCS(int cs)
{
	DEBUG_LOG_FMT(SP1, "AM-BB ChipSelect={:d}", cs);
	if (cs)
		m_position = 0;
}

bool CEXIAMBaseboard::IsPresent()
{
	return true;
}

void CEXIAMBaseboard::TransferByte(u8& _byte)
{
	DEBUG_LOG_FMT(SP1, "AM-BB > {:02x}", _byte);
	if (m_position < 4)
	{
		m_command[m_position] = _byte;
		_byte = 0xFF;
	}

	if ((m_position >= 2) && (m_command[0] == 0 && m_command[1] == 0))
	{
		// Read serial ID
		_byte = "\x06\x04\x10\x00"[(m_position-2)&3];
	}
	else if (m_position == 3)
	{
		unsigned int checksum = (m_command[0] << 24) | (m_command[1] << 16) | (m_command[2] << 8);
		unsigned int bit = 0x80000000UL;
		unsigned int check = 0x8D800000UL;
		while (bit >= 0x100)
		{
			if (checksum & bit)
				checksum ^= check;
			check >>= 1;
			bit >>= 1;
		}

		if (m_command[3] != (checksum & 0xFF))
			DEBUG_LOG_FMT(SP1, "AM-BB cs: {:02x}, w: {:02x}", m_command[3], checksum & 0xFF);
	}
	else
	{
		if (m_position == 4)
		{
			switch (m_command[0])
			{
			case 0x01:
				m_backoffset = (m_command[1] << 8) | m_command[2];
				DEBUG_LOG_FMT(SP1,"AM-BB COMMAND: Backup Offset:{:04x}", m_backoffset );
				m_backup->Seek( m_backoffset, File::SeekOrigin::Begin );
				_byte = 0x01;
				break;
			case 0x02:
				DEBUG_LOG_FMT(SP1,"AM-BB COMMAND: Backup Write:{:04x}-{:02x}", m_backoffset, m_command[1] );
				m_backup->WriteBytes( &m_command[1], 1 );
				m_backup->Flush();
				_byte = 0x01;
				break;
			case 0x03:
				DEBUG_LOG_FMT(SP1,"AM-BB COMMAND: Backup Read :{:04x}", m_backoffset );
				_byte = 0x01;
				break;
			// Unknown
			case 0x05:
				_byte = 0x04;
				break;
			// Clear IRQ
			case 0x82:
				WARN_LOG_FMT(SP1,"AM-BB COMMAND: 0x82 :{:02x} {:02x}", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0x83:
				WARN_LOG_FMT(SP1,"AM-BB COMMAND: 0x83 :{:02x} {:02x}", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown - 2 byte out
			case 0x86:
				WARN_LOG_FMT(SP1,"AM-BB COMMAND: 0x86 :{:02x} {:02x}", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0x87:
				WARN_LOG_FMT(SP1,"AM-BB COMMAND: 0x87 :{:02x} {:02x}", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0xFF:
				WARN_LOG_FMT(SP1,"AM-BB COMMAND: 0xFF :{:02x} {:02x}", m_command[1], m_command[2] );
				if( (m_command[1] == 0) && (m_command[2] == 0) )
				{
					m_have_irq = true;
					m_irq_timer = 0;
					m_irq_status = 0x02;
				}
				if( (m_command[1] == 2) && (m_command[2] == 1) )
				{
					m_irq_status = 0;
				}
				_byte = 0x04;
				break;
			default:
				_byte = 4;
				ERROR_LOG_FMT(SP1, "AM-BB COMMAND: {:02x} {:02x} {:02x}", m_command[0], m_command[1], m_command[2]);
				break;
			}
		}
		else if (m_position > 4)
		{
			switch (m_command[0])
			{
			// Read backup - 1 byte out
			case 0x03:
				m_backup->ReadBytes( &_byte, 1);
				break;
			// IMR - 2 byte out
			case 0x82:
				if(m_position == 6)
				{
					_byte = m_irq_status;
					m_have_irq = false;
				}
				else
				{
					_byte = 0x00;
				}
				break;
			// ? - 2 byte out
			case 0x86:
				_byte = 0x00;
				break;
			default:
				DEBUG_ASSERT_MSG(SP1, 0, "Unknown AM-BB command");
				break;
			}
		}
		else
		{
			_byte = 0xFF;
		}
	}
	DEBUG_LOG_FMT(SP1, "AM-BB < {:02x}", _byte);
	m_position++;
}

bool CEXIAMBaseboard::IsInterruptSet()
{
	if (m_have_irq)
	{
		DEBUG_LOG_FMT(SP1, "AM-BB IRQ");
		if( ++m_irq_timer > 4 )
			m_have_irq = false;
		return 1;
	}
	else
	{
		return 0;
	}
}

void CEXIAMBaseboard::DoState(PointerWrap &p)
{
	p.Do(m_position);
	p.Do(m_have_irq);
	p.Do(m_command);
}
CEXIAMBaseboard::~CEXIAMBaseboard()
{
	m_backup->Close();
	delete m_backup;
}
