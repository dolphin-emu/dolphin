// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Thread.h"

// Data structure to be passed to the flushing thread.
struct FlushData
{
	bool bExiting;
	std::string filename;
	u8 *memcardContent;
	int memcardSize, memcardIndex;
};

class CEXIMemoryCard : public IEXIDevice
{
public:
	CEXIMemoryCard(const int index);
	virtual ~CEXIMemoryCard();
	void SetCS(int cs) override;
	void Update() override;
	bool IsInterruptSet() override;
	bool IsPresent() override;
	void DoState(PointerWrap &p) override;
	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;
	IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1) override;

private:
	// This is scheduled whenever a page write is issued. The this pointer is passed
	// through the userdata parameter, so that it can then call Flush on the right card.
	static void FlushCallback(u64 userdata, int cyclesLate);

	// Scheduled when a command that required delayed end signaling is done.
	static void CmdDoneCallback(u64 userdata, int cyclesLate);

	// Flushes the memory card contents to disk.
	void Flush(bool exiting = false);

	// Signals that the command that was previously executed is now done.
	void CmdDone();

	// Variant of CmdDone which schedules an event later in the future to complete the command.
	void CmdDoneLater(u64 cycles);

	enum
	{
		cmdNintendoID       = 0x00,
		cmdReadArray        = 0x52,
		cmdArrayToBuffer    = 0x53,
		cmdSetInterrupt     = 0x81,
		cmdWriteBuffer      = 0x82,
		cmdReadStatus       = 0x83,
		cmdReadID           = 0x85,
		cmdReadErrorBuffer  = 0x86,
		cmdWakeUp           = 0x87,
		cmdSleep            = 0x88,
		cmdClearStatus      = 0x89,
		cmdSectorErase      = 0xF1,
		cmdPageProgram      = 0xF2,
		cmdExtraByteProgram = 0xF3,
		cmdChipErase        = 0xF4,
	};

	std::string m_strFilename;
	int card_index;
	int et_this_card, et_cmd_done;
	//! memory card state

	// STATE_TO_SAVE
	int interruptSwitch;
	bool m_bInterruptSet;
	int command;
	int status;
	u32 m_uPosition;
	u8 programming_buffer[128];
	u32 formatDelay;
	bool m_bDirty;
	//! memory card parameters
	unsigned int nintendo_card_id, card_id;
	unsigned int address;
	int memory_card_size; //! in bytes, must be power of 2.
	u8 *memory_card_content;

	FlushData flushData;
	std::thread flushThread;

protected:
	virtual void TransferByte(u8 &byte) override;
};
