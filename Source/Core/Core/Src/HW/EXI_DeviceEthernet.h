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

void DEBUGPRINT(const char * format, ...);

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
			DEBUGPRINT("Write too large!");
			exit(0);
		}

		memcpy(_buffer + _size, src, s);
		_size = _size + s;
	}
	void clear() { _size = 0; }
	u8* const p() { return _buffer; }

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

// TODO: convert into unions
enum
{
	BBA_RECV_SIZE	= 0x800,
	BBA_MEM_SIZE	= 0x1000,

	CB_OFFSET	= 0x100,
	CB_SIZE		= (BBA_MEM_SIZE - CB_OFFSET),
	SIZEOF_ETH_HEADER		= 0xe,
	SIZEOF_RECV_DESCRIPTOR	= 4,

	EXI_DEVTYPE_ETHER	= 0x04020200,

	BBA_NCRA			= 0x00, // Network Control Register A, RW
	BBA_NCRA_RESET		= 0x01, // RESET
	BBA_NCRA_ST0		= 0x02, // ST0, Start transmit command/status
	BBA_NCRA_ST1		= 0x04, // ST1,  "
	BBA_NCRA_SR			= 0x08, // SR, Start Receive

	BBA_NCRB					= 0x01, // Network Control Register B, RW
	BBA_NCRB_PR					= 0x01, // PR, Promiscuous Mode
	BBA_NCRB_CA					= 0x02, // CA, Capture Effect Mode
	BBA_NCRB_PM					= 0x04, // PM, Pass Multicast
	BBA_NCRB_PB					= 0x08, // PB, Pass Bad Frame
	BBA_NCRB_AB					= 0x10, // AB, Accept Broadcast
	BBA_NCRB_HBD				= 0x20, // HBD, reserved
	BBA_NCRB_RXINTC0			= 0x40, // RXINTC, Receive Interrupt Counter
	BBA_NCRB_RXINTC1			= 0x80, //  "
	BBA_NCRB_1_PACKET_PER_INT	= 0x00, // 0 0
	BBA_NCRB_2_PACKETS_PER_INT	= 0x40, // 0 1
	BBA_NCRB_4_PACKETS_PER_INT	= 0x80, // 1 0
	BBA_NCRB_8_PACKETS_PER_INT	= 0xC0, // 1 1

	BBA_IMR			= 0x08, // Interrupt Mask Register, RW, 00h

	BBA_IR			= 0x09, // Interrupt Register, RW, 00h
	BBA_IR_FRAGI	= 0x01, // FRAGI, Fragment Counter Interrupt
	BBA_IR_RI		= 0x02, // RI, Receive Interrupt
	BBA_IR_TI		= 0x04, // TI, Transmit Interrupt
	BBA_IR_REI		= 0x08, // REI, Receive Error Interrupt
	BBA_IR_TEI		= 0x10, // TEI, Transmit Error Interrupt
	BBA_IR_FIFOEI	= 0x20, // FIFOEI, FIFO Error Interrupt
	BBA_IR_BUSEI	= 0x40, // BUSEI, BUS Error Interrupt
	BBA_IR_RBFI		= 0x80, // RBFI, RX Buffer Full Interrupt

	BBA_NWAYC			= 0x30, // NWAY Configuration Register, RW, 84h
	BBA_NWAYC_FD		= 0x01, // FD, Full Duplex Mode
	BBA_NWAYC_PS100		= 0x02, // PS100/10, Port Select 100/10
	BBA_NWAYC_ANE		= 0x04, // ANE, Autonegotiation Enable
	BBA_NWAYC_ANS_RA	= 0x08, // ANS, Restart Autonegotiation
	BBA_NWAYC_LTE		= 0x80, // LTE, Link Test Enable

	BBA_NWAYS			= 0x31,
	BBA_NWAYS_LS10		= 0x01,
	BBA_NWAYS_LS100		= 0x02,
	BBA_NWAYS_LPNWAY	= 0x04,
	BBA_NWAYS_ANCLPT	= 0x08,
	BBA_NWAYS_100TXF	= 0x10,
	BBA_NWAYS_100TXH	= 0x20,
	BBA_NWAYS_10TXF		= 0x40,
	BBA_NWAYS_10TXH		= 0x80,

	BBA_INTERRUPT_RECV			= 0x02,
	BBA_INTERRUPT_SENT			= 0x04,
	BBA_INTERRUPT_RECV_ERROR	= 0x08,
	BBA_INTERRUPT_SEND_ERROR	= 0x10,

	BBA_RWP					 = 0x16, // Receive Buffer Write Page Pointer Register
	BBA_RRP					 = 0x18, // Receive Buffer Read Page Pointer Register

	BBA_SI_ACTRL2			 = 0x60
};

enum
{
	EXPECT_NONE = 0,
	EXPECT_ID
};

class CEXIETHERNET : public IEXIDevice
{
public:
	CEXIETHERNET();
	~CEXIETHERNET();
	void SetCS(int _iCS);
	bool IsPresent();
	void Update();
	bool IsInterruptSet();
	void ImmWrite(u32 _uData,  u32 _uSize);
	u32  ImmRead(u32 _uSize);
	void DMAWrite(u32 _uAddr, u32 _uSize);
	void DMARead(u32 _uAddr, u32 _uSize);

//private:
	// STATE_TO_SAVE
	u32 m_uPosition;
	u32 m_uCommand;

	bool m_bInterruptSet;
	u32 mWriteP, mReadP;

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
