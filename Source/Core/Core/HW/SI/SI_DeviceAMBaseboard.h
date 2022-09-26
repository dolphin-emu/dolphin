// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _SIDEVICE_AMBASEBOARD_H
#define _SIDEVICE_AMBASEBOARD_H

// triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard : public SerialInterface::ISIDevice
{
private:
	enum EBufferCommands
	{
		CMD_RESET		= 0x00,
		CMD_GCAM		= 0x70,
	};
	enum CARDCommands
	{
		CARD_INIT			= 0x10,
		CARD_GET_CARD_STATE	= 0x20,
		CARD_IS_PRESENT		= 0x40,
		CARD_LOAD_CARD		= 0xB0,
		CARD_CLEAN_CARD		= 0xA0,
		CARD_READ			= 0x33,
		CARD_WRITE			= 0x53,
		CARD_WRITE_INFO		= 0x7C,
		CARD_78				= 0x78,
		CARD_7A				= 0x7A,
		CARD_7D				= 0x7D,
		CARD_D0				= 0xD0,
		CARD_80				= 0x80,
	};

	unsigned short m_coin[2];
	int m_coin_pressed[2];

	u8	m_card_memory[0xD0];
	u8	m_card_read_packet[0xDB];
	u8  m_card_buffer[0x100];
	u32 m_card_memory_size;
	u32 m_card_is_inserted;
	u32 m_card_command;
	u32 m_card_clean;
	u32 m_card_write_length;
	u32 m_card_wrote;
	u32 m_card_read_length;
	u32 m_card_read;
	u32 m_card_bit;
	u32 m_card_state_call_count;
	u8  m_card_offset=0;

	u32 m_wheelinit;

	u32 m_motorinit;
	u8  m_motorreply[6];
	u32 m_motorforce;
	s16 m_motorforce_x;
	s16 m_motorforce_y;

public:
	// constructor
	CSIDevice_AMBaseboard(SerialInterface::SIDevices device, int _iDeviceNumber);

	// run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// send a command directly
	virtual void SendCommand(u32 _Cmd, u8 _Poll);
};

#endif // _SIDEVICE_AMBASEBOARD_H
