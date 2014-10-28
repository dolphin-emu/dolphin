// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#include "Common/StdMakeUnique.h"
#include "Core/HW/EXI_Device.h"

class MemoryCardBase;
class CEXIMemoryCard : public IEXIDevice
{
public:
	CEXIMemoryCard(const int index, bool gciFolder);
	virtual ~CEXIMemoryCard();
	void SetCS(int cs) override;
	bool IsInterruptSet() override;
	bool UseDelayedTransferCompletion() override;
	bool IsPresent() override;
	void DoState(PointerWrap &p) override;
	IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1) override;
	void DMARead(u32 _uAddr, u32 _uSize) override;
	void DMAWrite(u32 _uAddr, u32 _uSize) override;

private:
	void SetupGciFolder(u16 sizeMb);
	void SetupRawMemcard(u16 sizeMb);
	static void EventCompleteFindInstance(u64 userdata, std::function<void(CEXIMemoryCard*)> callback);

	// Scheduled when a command that required delayed end signaling is done.
	static void CmdDoneCallback(u64 userdata, int cyclesLate);

	// Scheduled when memory card is done transferring data
	static void TransferCompleteCallback(u64 userdata, int cyclesLate);

	// Signals that the command that was previously executed is now done.
	void CmdDone();

	// Signals that the transfer that was previously executed is now done.
	void TransferComplete();

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

	int card_index;
	int et_cmd_done, et_transfer_complete;
	//! memory card state

	// STATE_TO_SAVE
	int interruptSwitch;
	bool m_bInterruptSet;
	int command;
	int status;
	u32 m_uPosition;
	u8 programming_buffer[128];
	//! memory card parameters
	unsigned int card_id;
	unsigned int address;
	u32 memory_card_size;
	std::unique_ptr<MemoryCardBase> memorycard;

protected:
	virtual void TransferByte(u8 &byte) override;
};
