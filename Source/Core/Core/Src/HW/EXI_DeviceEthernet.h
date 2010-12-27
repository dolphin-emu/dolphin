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

#ifndef _EXIDEVICE_ETHERNET_H
#define _EXIDEVICE_ETHERNET_H

#include "Thread.h"

inline u8 makemaskb(int start, int end) {
	return (u8)_rotl((2 << (end - start)) - 1, 7 - end);
}
inline u32 makemaskh(int start, int end) {
	return (u32)_rotl((2 << (end - start)) - 1, 15 - end);
}
inline u32 makemaskw(int start, int end) {
	return _rotl((2 << (end - start)) - 1, 31 - end);
}
inline u8 getbitsb(u8 byte, int start, int end) {
	return (byte & makemaskb(start, end)) >> u8(7 - end);
}
inline u32 getbitsh(u32 hword, int start, int end) {
	return (hword & makemaskh(start, end)) >> u32(15 - end);
}
inline u32 getbitsw(u32 dword, int start, int end) {
	return (dword & makemaskw(start, end)) >> (31 - end);
}

class WriteBuffer
{
public:
	WriteBuffer(u32 s) : _size(0)
	{
		_buffer = (u8*)malloc(s*sizeof(u8));
		ucapacity = s;
	}
	~WriteBuffer() { free(_buffer); }
	u32 size() const { return _size; }
	u32 capacity() const { return ucapacity; }
	void write(u32 s, const void *src)
	{
		if (_size + s >= ucapacity)
		{
			INFO_LOG(SP1, "Write too large!");
			exit(0);
		}

		memcpy(_buffer + _size, src, s);
		_size = _size + s;
	}
	void clear() { _size = 0; }
	u8* p() { return _buffer; }

private:
	u8* _buffer;
	u32 ucapacity;
	u32 _size;
};

// Doesn't contain error checks for wraparound writes
class CyclicBufferWriter 
{
public:
	CyclicBufferWriter(u8 *buffer, size_t cap) {
		_buffer = buffer; _capacity = cap; _write = 0;
	}

	size_t p_write() const { return _write; }
	void reset() { _write = 0; }

	void write(void *src, size_t size) {
		_dbg_assert_(SP1, size < _capacity);
		u8* bsrc = (u8*) src;
		if (_write + size >= _capacity)
		{
			// wraparound
			memcpy(_buffer + _write, src, _capacity - _write);
			memcpy(_buffer, bsrc + (_capacity - _write), size - (_capacity - _write));
			_write = size - (_capacity - _write);
		} else {
			memcpy(_buffer + _write, src, size);
			_write += size;
		}
		//DEBUG_LOG(SP1, "CBWrote %i bytes", size);
	}
	// Aligns the write pointer to steps of 0x100, like the real BBA
	void align() {
		_write = (_write + 0xff) & ~0xff;
		if(_write >= _capacity)
			_write -= _capacity;
	}

private:
	size_t _write;
	size_t _capacity;
	u8 *_buffer;
};

#define INVALID_P 0xFFFF

// Network Control Register A, RW
enum NCRA
{
	NCRA_RESET		= 0x01, // RESET
	NCRA_ST0		= 0x02, // Start transmit command/status
	NCRA_ST1		= 0x04, // "
	NCRA_SR			= 0x08, // Start Receive
};

// Network Control Register B, RW
enum NCRB
{
	NCRB_PR			= 0x01, // Promiscuous Mode
	NCRB_CA			= 0x02, // Capture Effect Mode
	NCRB_PM			= 0x04, // Pass Multicast
	NCRB_PB			= 0x08, // Pass Bad Frame
	NCRB_AB			= 0x10, // Accept Broadcast
	NCRB_HBD		= 0x20, // reserved
	NCRB_RXINTC		= 0xC0, // Receive Interrupt Counter (mask)
};

// Interrupt Mask Register, RW, 00h
// Interrupt Register, RW, 00h
enum Interrupts
{
	INT_FRAG		= 0x01, // Fragment Counter
	INT_R			= 0x02, // Receive
	INT_T			= 0x04, // Transmit
	INT_R_ERR		= 0x08, // Receive Error
	INT_T_ERR		= 0x10, // Transmit Error
	INT_FIFO_ERR	= 0x20, // FIFO Error
	INT_BUS_ERR		= 0x40, // BUS Error
	INT_RBF			= 0x80, // RX Buffer Full
};

// NWAY Configuration Register, RW, 84h
enum NWAYC
{
	NWAYC_FD		= 0x01, // Full Duplex Mode
	NWAYC_PS100		= 0x02, // Port Select 100/10
	NWAYC_ANE		= 0x03, // Autonegotiation Enable
	NWAYC_ANS_RA	= 0x04, // Restart Autonegotiation
	NWAYC_LTE		= 0x08, // Link Test Enable
};

enum NWAYS
{
	NWAYS_LS10		= 0x01,
	NWAYS_LS100		= 0x02,
	NWAYS_LPNWAY	= 0x04,
	NWAYS_ANCLPT	= 0x08,
	NWAYS_100TXF	= 0x10,
	NWAYS_100TXH	= 0x20,
	NWAYS_10TXF		= 0x40,
	NWAYS_10TXH		= 0x80
};

enum
{
	BBA_NCRA		= 0x00,
	BBA_NCRB		= 0x01,
	
	BBA_LTPS		= 0x04,
	BBA_LRPS		= 0x05,
	
	BBA_IMR			= 0x08,
	BBA_IR			= 0x09,
	
	BBA_BP			= 0x0a,
	BBA_TLBP		= 0x0c,
	BBA_TWP			= 0x0e,
	BBA_TRP			= 0x12,
	BBA_RWP			= 0x16,
	BBA_RRP			= 0x18,
	BBA_RHBP		= 0x1a,

	BBA_RXINTT		= 0x14,

	BBA_NAFR_PAR0	= 0x20,
	BBA_NAFR_PAR1	= 0x21,
	BBA_NAFR_PAR2	= 0x22,
	BBA_NAFR_PAR3	= 0x23,
	BBA_NAFR_PAR4	= 0x24,
	BBA_NAFR_PAR5	= 0x25,

	BBA_NWAYC		= 0x30,
	BBA_NWAYS		= 0x31,

	BBA_GCA			= 0x32,

	BBA_MISC		= 0x3d,

	BBA_TXFIFOCNT	= 0x3e,
	BBA_WRTXFIFOD	= 0x48,

	BBA_MISC2		= 0x50,

	BBA_SI_ACTRL	= 0x5c,
	BBA_SI_STATUS	= 0x5d,
	BBA_SI_ACTRL2	= 0x60,
};

enum
{
	BBA_RECV_SIZE	= 0x800,
	BBA_MEM_SIZE	= 0x1000,

	CB_OFFSET	= 0x100,
	CB_SIZE		= (BBA_MEM_SIZE - CB_OFFSET),
	SIZEOF_ETH_HEADER		= 0xe,
	SIZEOF_RECV_DESCRIPTOR	= 4,

	EXI_DEVTYPE_ETHER	= 0x04020200,
};

enum
{
	EXPECT_NONE = 0,
	EXPECT_ID
};

static u32 mPacketsSent = 0;

class CEXIETHERNET : public IEXIDevice
{
public:
	CEXIETHERNET(const std::string& mac_addr);
	~CEXIETHERNET();
	void SetMAC(u8 *new_addr);
	void SetCS(int _iCS);
	bool IsPresent();
	void Update();
	bool IsInterruptSet();
	void ImmWrite(u32 data,  u32 size);
	u32  ImmRead(u32 size);
	void DMAWrite(u32 addr, u32 size);
	void DMARead(u32 addr, u32 size);

//private:
	// STATE_TO_SAVE
	u32 m_uPosition;
	u32 m_uCommand;

	bool m_bInterruptSet;
	u16 mWriteP, mReadP;

	bool mExpectSpecialImmRead;	//reset to false on deselect
	u32 mSpecialImmData;
	bool Activated;

	u16 mRBRPP;  //RRP - Receive Buffer Read Page Pointer
	bool mRBEmpty;

	u8 mBbaMem[BBA_MEM_SIZE];

	WriteBuffer mWriteBuffer;
	CyclicBufferWriter mCbw;

	bool mExpectVariableLengthImmWrite;
	bool mReadyToSend;
	u8 RegisterBlock[0x1000];
	enum
	{
		CMD_ID = 0x00,
		CMD_READ_REG = 0x01,
	};

	void recordSendComplete();
	bool sendPacket(u8 *etherpckt, int size);
	bool checkRecvBuffer();
	bool handleRecvdPacket();

	//TAP interface
	bool activate();
	bool CheckRecieved();
	bool deactivate();
	bool isActivated();
	bool resume();
	bool startRecv();
	bool cbwriteDescriptor(u32 size);


	volatile bool mWaiting;
	static const u8 mac_address_default[6];
	u8 mac_address[6];
	u8 mRecvBuffer[BBA_RECV_SIZE];
#ifdef _WIN32
	HANDLE mHAdapter, mHRecvEvent, mHReadWait;
	DWORD mMtu;
	OVERLAPPED mReadOverlapped;
	DWORD mRecvBufferLength;
	static VOID CALLBACK ReadWaitCallback(PVOID lpParameter, BOOLEAN TimerFired);
#else
	u32 mRecvBufferLength;
#endif
};

#endif
