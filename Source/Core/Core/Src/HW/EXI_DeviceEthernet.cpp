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

#include "Memmap.h"
//#pragma optimize("",off)
#include "EXI_Device.h"
#include "EXI_DeviceEthernet.h"

#define MAKE(type, arg) (*(type *)&(arg))

const u8 CEXIETHERNET::mac_address_default[6] = { 0x00, 0x09, 0xbf, 0x01, 0x00, 0xc1 };

CEXIETHERNET::CEXIETHERNET(const std::string& mac_addr) :
	m_uPosition(0),
	m_uCommand(0),
	mWriteBuffer(2048),
	mCbw(mBbaMem + CB_OFFSET, CB_SIZE)
{
	memset(mBbaMem, 0, BBA_MEM_SIZE);
	
	int x = 0;
	u8 new_addr[6] = { 0 };
	for (int i = 0; i < mac_addr.size() && x < 12; i++)
	{
		char c = mac_addr.at(i);
		if (c >= '0' && c <= '9') {
			new_addr[x / 2] |= (c - '0') << ((x & 1) ? 0 : 4); x++;
		} else if (c >= 'A' && c <= 'F') {
			new_addr[x / 2] |= (c - 'A' + 10) << ((x & 1) ? 0 : 4); x++;
		} else if (c >= 'a' && c <= 'f') {
			new_addr[x / 2] |= (c - 'a' + 10) << ((x & 1) ? 0 : 4); x++;
		}
	}
	if (x / 2 == 6)
		memcpy(mac_address, new_addr, 6);
	else
		memcpy(mac_address, mac_address_default, 6);

	ERROR_LOG(SP1, "BBA MAC %x%x%x%x%x%x",
		mac_address[0], mac_address[1], mac_address[2],
		mac_address[3], mac_address[4], mac_address[5]);

	mWriteP = INVALID_P;
	mReadP = INVALID_P;
	mWaiting = false;
	mReadyToSend = false;
	Activated = false;
	m_bInterruptSet = false;

#ifdef _WIN32
	mHAdapter = INVALID_HANDLE_VALUE;
	mHRecvEvent = INVALID_HANDLE_VALUE;
	mHReadWait = INVALID_HANDLE_VALUE;
#endif

	mRecvBufferLength = 0;

	mExpectSpecialImmRead = false;
	mExpectVariableLengthImmWrite = false;
}

CEXIETHERNET::~CEXIETHERNET()
{
	/*/ crashy crashy
	if (isActivated())
		deactivate();
	//*/
}

void CEXIETHERNET::SetCS(int cs)
{
	INFO_LOG(SP1, "chip select: %s%s", cs ? "true" : "false",
		mExpectVariableLengthImmWrite ? ", expecting variable write" : "");
	if (!cs)
	{
		if (mExpectVariableLengthImmWrite)
		{
			INFO_LOG(SP1, "Variable write complete. Final size: %i bytes",
				mWriteBuffer.size());
			mExpectVariableLengthImmWrite = false;
			mReadyToSend = true;
		}
		mExpectSpecialImmRead = false;
		mWriteP = mReadP = INVALID_P;
	}
}

bool CEXIETHERNET::IsPresent()
{
	return true;
}

void CEXIETHERNET::Update()
{
	return;
}

bool CEXIETHERNET::IsInterruptSet()
{
	return m_bInterruptSet;
}

void CEXIETHERNET::recordSendComplete()
{
	mBbaMem[BBA_NCRA] &= ~(NCRA_ST0 | NCRA_ST1);
	if (mBbaMem[BBA_IMR] & INT_T)
	{
		mBbaMem[BBA_IR] |= INT_T;
		INFO_LOG(SP1, "\t\tBBA Send interrupt raised");
		m_bInterruptSet = true;
	}
	// TODO why did sonic put this here?
	//startRecv();
	mPacketsSent++;
}

bool CEXIETHERNET::checkRecvBuffer()
{
	if (mRecvBufferLength)
		handleRecvdPacket();
	return true;
}

void CEXIETHERNET::ImmWrite(u32 data, u32 size)
{
	if (size == 1)
		data = (u8)Common::swap32(data);
	else if (size == 2)
		data >>= 16;

	INFO_LOG(SP1, "imm write %0*x writep 0x%x", size * 2, data, mWriteP);

	if (mExpectVariableLengthImmWrite) 
	{
		INFO_LOG(SP1, "\tvariable length imm write");
		if (size == 4)
			data = Common::swap32(data);
		else if (size == 2)
			data = Common::swap16((u16)data);
		mWriteBuffer.write(size, &data);
		return;
	}
	else if (mWriteP != INVALID_P)
	{
		if (mWriteP + size > BBA_MEM_SIZE)
		{
			ERROR_LOG(SP1, "write error: writep + size = %04x + %i", mWriteP, size);
			return;
		}
		INFO_LOG(SP1, "\twrite to BBA address %0*x, %i byte%s: %0*x",
			mWriteP >= CB_OFFSET ? 4 : 2, mWriteP, size, (size==1?"":"s"), size*2, data);

		switch (mWriteP)
		{
		case BBA_NCRA:
			{
				INFO_LOG(SP1, "\t\tNCRA");

				// TODO is it really necessary to check last value?
				u8 NCRA_old = mBbaMem[BBA_NCRA];
				mBbaMem[BBA_NCRA] = data;
				#define RISE(flags) ((mBbaMem[BBA_NCRA] & flags) && !(NCRA_old & flags))

				if (RISE(NCRA_RESET))
				{
					INFO_LOG(SP1, "\t\treset");
					activate();
				}
				if (RISE(NCRA_SR))
				{
					INFO_LOG(SP1, "\t\tstart receive");
					startRecv();
				}
				if (RISE(NCRA_ST1)) 
				{
					INFO_LOG(SP1, "\t\tstart transmit");
					if (!mReadyToSend)
					{
						INFO_LOG(SP1, "\t\ttramsit without a packet!");
					}
					sendPacket(mWriteBuffer.p(), mWriteBuffer.size());
					mReadyToSend = false;
				}
			}
			break;
		case 0x03:
			mBbaMem[0x03] = ~data;
			if (data & 0x80)
				m_bInterruptSet = false;
			break;
		case BBA_IR:
			_dbg_assert_(SP1, size == 1);
			INFO_LOG(SP1, "\t\tinterrupt reset %02x & ~(%02x) => %02x",
				mBbaMem[BBA_IR], MAKE(u8, data), mBbaMem[BBA_IR] & ~MAKE(u8, data));
			mBbaMem[BBA_IR] &= ~MAKE(u8, data);
			break;
		case BBA_RWP:	// RWP - Receive Buffer Write Page Pointer
			INFO_LOG(SP1, "\t\tRWP");
			_dbg_assert_(SP1, size == 2 || size == 1);
			//_dbg_assert_(SP1, data == (u32)((u16)mCbw.p_write() + CB_OFFSET) >> 8);
			if (data != (u32)((u16)mCbw.p_write() + CB_OFFSET) >> 8)
			{
				ERROR_LOG(SP1, "BBA RWP ASSERT data %x p_write %x", data, mCbw.p_write());
			}
			break;
		case BBA_RRP:	// RRP - Receive Buffer Read Page Pointer
			INFO_LOG(SP1, "\t\tRRP");
			_dbg_assert_(SP1, size == 2 || size == 1);
			mRBRPP = (u8)data << 8;	// Hope this works with both write sizes.
			mRBEmpty = mRBRPP == ((u16)mCbw.p_write() + CB_OFFSET);
			checkRecvBuffer();
			break;
		case BBA_NWAYC:
			INFO_LOG(SP1, "\t\tNWAYC");
			mBbaMem[BBA_NWAYC] = data;
			if (mBbaMem[BBA_NWAYC] & (NWAYC_ANE | NWAYC_ANS_RA))
			{
				// say we've successfully negotiated for 10 Mbit full duplex
				// should placate libogc
				mBbaMem[BBA_NWAYS] = NWAYS_LS100 | NWAYS_LPNWAY | NWAYS_ANCLPT | NWAYS_100TXF;
			}
			break;
		default:
			INFO_LOG(SP1, "\t\tdefault: data: %0*x to %x", size * 2, data, mWriteP);
			memcpy(mBbaMem + mWriteP, &data, size);
			mWriteP = mWriteP + (u16)size;
		}
		return;
	}
	else if (size == 2 && data == 0)
	{
		INFO_LOG(SP1, "\trequest cid");
		mSpecialImmData = EXI_DEVTYPE_ETHER;
		mExpectSpecialImmRead = true;
		return;
	}
	else if ((size == 4 && (data & 0xC0000000) == 0xC0000000) ||
		(size == 2 && (data & 0x4000) == 0x4000))
	{
		INFO_LOG(SP1, "\twrite to register");
		if (size == 4)
			mWriteP = (u8)getbitsw(data, 16, 23);
		else //size == 2
			mWriteP = (u8)getbitsw(data & ~0x4000, 16, 23); // Dunno about this...

		if (mWriteP == BBA_WRTXFIFOD)
		{
			mWriteBuffer.clear();
			mExpectVariableLengthImmWrite = true;
			INFO_LOG(SP1, "\t\tprepared for variable length write to address 0x48");
		}
		else
		{
			INFO_LOG(SP1, "\t\twritep set to %0*x", size * 2, mWriteP);
		}
		return;
	}
	else if ((size == 4 && (data & 0xC0000000) == 0x80000000) ||
		(size == 2 && (data & 0x4000) == 0x0000))
	{
		// Read from BBA Register
		if (size == 4)
			mReadP = (data >> 8) & 0xffff;
		else //size == 2
			mReadP = (u8)getbitsw(data, 16, 23);

		INFO_LOG(SP1, "Read from BBA register 0x%x", mReadP);

		switch (mReadP)
		{
		case BBA_NCRA:
			// These Two lines were commented out in Whinecube
			//mBbaMem[mReadP] = 0x00;
			//if(!sendInProgress())
			mBbaMem[BBA_NCRA] &= ~(NCRA_ST0 | NCRA_ST1);
			INFO_LOG(SP1, "\tNCRA %02x", mBbaMem[BBA_NCRA]);
			break;
		case BBA_NCRB:	// Revision ID
			INFO_LOG(SP1, "\tNCRB");
			break;
		case BBA_NAFR_PAR0:
			INFO_LOG(SP1, "\tMac Address");
			memcpy(mBbaMem + mReadP, mac_address, 6);
			break;
		case 0x03: // status TODO more fields
			mBbaMem[mReadP] = m_bInterruptSet ? 0x80 : 0;
			INFO_LOG(SP1, "\tStatus", mBbaMem[mReadP]);
			break;
		case BBA_LTPS:
			INFO_LOG(SP1, "\tLPTS");
			break;
		case BBA_IMR:
			INFO_LOG(SP1, "\tIMR");
			break;
		case BBA_IR:
			INFO_LOG(SP1, "\tIR");
			break;
		case BBA_RWP:
		case BBA_RWP+1:
			MAKE(u16, mBbaMem[BBA_RWP]) = Common::swap16((u16)mCbw.p_write() + CB_OFFSET);
			INFO_LOG(SP1, "\tRWP 0x%04x", MAKE(u16, mBbaMem[mReadP]));
			break;
		case BBA_RRP:
		case BBA_RRP+1:
			MAKE(u16, mBbaMem[BBA_RRP]) = Common::swap16(mRBRPP);
			INFO_LOG(SP1, "\tRRP 0x%04x", MAKE(u16, mBbaMem[mReadP]));
			break;
		case 0x3A:	// bit 1 set if no data available
			INFO_LOG(SP1, "\tBit 1 set!");
			//mBbaMem[mReadP] = !mRBEmpty;
			break;
		case BBA_TWP:
		case BBA_TWP+1:
			INFO_LOG(SP1, "\tTWP");
			break;
		case BBA_NWAYC:
			INFO_LOG(SP1, "\tNWAYC");
			break;
		case BBA_NWAYS:
			mBbaMem[BBA_NWAYS] = NWAYS_LS100 | NWAYS_LPNWAY | NWAYS_ANCLPT | NWAYS_100TXF;
			INFO_LOG(SP1, "\tNWAYS %02x", mBbaMem[BBA_NWAYS]);
			break;
		case BBA_SI_ACTRL:
			INFO_LOG(SP1, "\tSI_ACTRL");
			break;
		default:
			ERROR_LOG(SP1, "UNKNOWN BBA REG READ %02x", mReadP);
			break;
		}
		return;
	}

	ERROR_LOG(SP1, "\tNot expecting imm write of size %d", size);
	ERROR_LOG(SP1, "\t\t SKIPPING!");
}

u32 CEXIETHERNET::ImmRead(u32 size)
{
	INFO_LOG(SP1, "imm read %i readp %x", size, mReadP);

	if (mExpectSpecialImmRead)
	{
		INFO_LOG(SP1, "\tspecial imm read %08x", mSpecialImmData);
		mExpectSpecialImmRead = false;
		return mSpecialImmData;
	}

	if (mReadP != INVALID_P)
	{
		if (mReadP + size > BBA_MEM_SIZE)
		{
			ERROR_LOG(SP1, "\tRead error: readp + size = %04x + %i", mReadP, size);
			return 0;
		}
		u32 uResult = 0;
		memcpy(&uResult, mBbaMem + mReadP, size);

		uResult = Common::swap32(uResult);

		INFO_LOG(SP1, "\tRead from BBA address %0*x, %i byte%s: %0*x",
			mReadP >= CB_OFFSET ? 4 : 2, mReadP,
			size, (size==1?"":"s"),size*2, getbitsw(uResult, 0, size * 8 - 1));
		mReadP = mReadP + size;
		return uResult;
	}
	else
	{
		ERROR_LOG(SP1, "Unhandled imm read size %d", size);
		return 0;
	}
}

void CEXIETHERNET::DMAWrite(u32 addr, u32 size)
{
	if (mExpectVariableLengthImmWrite)
	{
		INFO_LOG(SP1, "DMA Write: Address is 0x%x and size is 0x%x", addr, size);
		mWriteBuffer.write(size, Memory::GetPointer(addr));
		return;
	}

	ERROR_LOG(SP1, "Unhandled BBA DMA write: %i, %08x", size, addr);
}

void CEXIETHERNET::DMARead(u32 addr, u32 size)
{
	if (mReadP != INVALID_P)
	{
		if (mReadP + size > BBA_MEM_SIZE)
		{
			INFO_LOG(SP1, "Read error: mReadP + size = %04x + %i", mReadP, size);
			return;
		}
		memcpy(Memory::GetPointer(addr), mBbaMem + mReadP, size);
		INFO_LOG(SP1, "DMA Read from BBA address %0*x, %i bytes to %08x",
			mReadP >= CB_OFFSET ? 4 : 2, mReadP, size, addr);
		mReadP = mReadP + (u16)size;
		return;
	}
	
	ERROR_LOG(SP1, "Unhandled BBA DMA read: %i, %08x", size, addr);
}
//#pragma optimize("",on)