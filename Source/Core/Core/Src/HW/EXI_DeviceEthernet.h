// Copyright (C) 2003-2009 Dolphin Project.

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



class WriteBuffer {
public:
	WriteBuffer(u32 s) :_size(0) { _buffer = (u8*)malloc(s*sizeof(u8)); ucapacity = s;}
	~WriteBuffer() { free(_buffer);}
	u32 size() const { return _size; }
	u32 capacity() const { return ucapacity; }
	void write(u32 s, const void *src) {
		if(_size + s >= ucapacity)
		{
			printf("Write too large!");
			exit(0);
		}

		memcpy(_buffer + _size, src, s);
		_size = _size + s;
	}
	void clear() {
		_size = 0;
	}
	u8* const p() { return _buffer; }
private:
	u8* _buffer;
	u32 ucapacity;
	u32 _size;
};

//Doesn't contain error checks for wraparound writes
class CyclicBufferWriter 
{
	public:
		CyclicBufferWriter(u8 *buffer, size_t cap) 
		{
			_buffer = buffer; _cap = cap; _write = 0;
		}

		size_t p_write() const { return _write; }
		void reset() { _write = 0; }

		void write(void *src, size_t size);
		void align();	//aligns the write pointer to steps of 0x100, like the real BBA
	private:
		size_t _write;
		size_t _cap;	//capacity
		u8 *_buffer;
};

class CEXIETHERNET : public IEXIDevice
{
public:
	CEXIETHERNET();
	void SetCS(int _iCS);
	bool IsPresent();
	void Update();
	bool IsInterruptSet();
	void ImmWrite(u32 _uData,  u32 _uSize);
	u32  ImmRead(u32 _uSize);
	void DMAWrite(u32 _uAddr, u32 _uSize);
	void DMARead(u32 _uAddr, u32 _uSize);

private:
	// STATE_TO_SAVE
	u32 m_uPosition;
	u32 m_uCommand;
	
	u32 mWriteP, mReadP;
	#define INVALID_P 0xFFFF
	
	bool mExpectSpecialImmRead;	//reset to false on deselect
	u32 mSpecialImmData;
	bool Activated;
	
	u16 mRBRPP;  //RRP - Receive Buffer Read Page Pointer
	bool mRBEmpty;
	
	u32 mRecvBufferLength;
	
	#define BBAMEM_SIZE 0x1000
	u8 mBbaMem[BBAMEM_SIZE];
	
	WriteBuffer mWriteBuffer;
	CyclicBufferWriter mCbw;
	
	bool mExpectVariableLengthImmWrite;
	bool mReadyToSend;
	unsigned int ID;
	u8 RegisterBlock[0x1000];
	enum {
		CMD_ID = 0x00,
		CMD_READ_REG = 0x01,
	};

	void recordSendComplete();
	bool sendPacket(u8 *etherpckt, int size);
	bool checkRecvBuffer();
	bool handleRecvdPacket();
	
	//TAP interface
	bool activate();
	bool deactivate();
	bool isActivated();

};
enum {
	EXPECT_NONE = 0,
	EXPECT_ID,
};
enum{
	CB_OFFSET = 0x100,
	CB_SIZE = (BBAMEM_SIZE - CB_OFFSET),
	SIZEOF_RECV_DESCRIPTOR = 4,

	EXI_DEVTYPE_ETHER =		0x04020200,
	BBA_NCRA		  =		0x00,		/* Network Control Register A, RW */
	BBA_NCRA_RESET	  =		(1<<0),	/* RESET */
	BBA_NCRA_ST0	  =		(1<<1),	/* ST0, Start transmit command/status */
	BBA_NCRA_ST1	  =		(1<<2),	/* ST1,  " */
	BBA_NCRA_SR		  =		(1<<3),	/* SR, Start Receive */
	BBA_NCRB		  =		0x01,		/* Network Control Register B, RW */
	BBA_NCRB_PR		  =		(1<<0),	/* PR, Promiscuous Mode */
	BBA_NCRB_CA		  =		(1<<1),	/* CA, Capture Effect Mode */
	BBA_NCRB_PM		  =		(1<<2),	/* PM, Pass Multicast */
	BBA_NCRB_PB		  =		(1<<3),	/* PB, Pass Bad Frame */
	BBA_NCRB_AB		  =		(1<<4),	/* AB, Accept Broadcast */
	BBA_NCRB_HBD	  =		(1<<5),	/* HBD, reserved */
	BBA_NCRB_RXINTC0  =		(1<<6),	/* RXINTC, Receive Interrupt Counter */
	BBA_NCRB_RXINTC1  =		(1<<7),	/*  " */
	BBA_NCRB_1_PACKET_PER_INT  = (0<<6),	/* 0 0 */
	BBA_NCRB_2_PACKETS_PER_INT = (1<<6),	/* 0 1 */
	BBA_NCRB_4_PACKETS_PER_INT = (2<<6),	/* 1 0 */
	BBA_NCRB_8_PACKETS_PER_INT = (3<<6),	/* 1 1 */

	BBA_NWAYC		 = 0x30,		/* NWAY Configuration Register, RW, 84h */
	BBA_NWAYC_FD	 = (1<<0),	/* FD, Full Duplex Mode */
	BBA_NWAYC_PS100  = (1<<1),	/* PS100/10, Port Select 100/10 */
	BBA_NWAYC_ANE    = (1<<2),	/* ANE, Autonegotiation Enable */
	BBA_NWAYC_ANS_RA = (1<<3),	/* ANS, Restart Autonegotiation */
	BBA_NWAYC_LTE    = (1<<7),	/* LTE, Link Test Enable */

	BBA_NWAYS        =	 0x31,
	BBA_NWAYS_LS10   =	 (1<<0),
	BBA_NWAYS_LS100  =	 (1<<1),
	BBA_NWAYS_LPNWAY =   (1<<2),
	BBA_NWAYS_ANCLPT =	 (1<<3),
	BBA_NWAYS_100TXF =	 (1<<4),
	BBA_NWAYS_100TXH =	 (1<<5),
	BBA_NWAYS_10TXF  =	 (1<<6),
	BBA_NWAYS_10TXH  =	 (1<<7),
    BBA_INTERRUPT_RECV = 0x02,
	BBA_INTERRUPT_SENT = 0x04,
	BBA_INTERRUPT_RECV_ERROR = 0x08,
	BBA_INTERRUPT_SEND_ERROR = 0x10,
};

#endif
