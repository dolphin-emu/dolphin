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

#include "EXI_Device.h"
#include "EXI_DeviceEthernet.h"

//#define SONICDEBUG
#ifdef SONICDEBUG
#define FILEDEBUG
#ifdef FILEDEBUG
FILE *ME = 0;
#endif
#endif
void DEBUGPRINT (const char * format, ...)
{
	char buffer[0x1000];
	va_list args;
	va_start(args, format);
	vsprintf(buffer,format, args);
#ifdef SONICDEBUG
#ifdef FILEDEBUG
	fprintf(ME, "%s\n",  buffer);
#endif
	printf("%s\n", buffer);
#else
	INFO_LOG(SP1, buffer);
#endif
	va_end(args);
}


#define MAKE(type, arg) (*(type *)&(arg))

#define RISE(flags) ((SwappedData & (flags)) && !(mBbaMem[0x00] & (flags)))

static u32 mPacketsSent = 0;
static u8 mac_address[6] = {0x00, 0x09, 0xbf, 0x01, 0x00, 0xc1}; // (shuffle2) from my bba
static u32 Expecting;

CEXIETHERNET::CEXIETHERNET() :
	m_uPosition(0),
	m_uCommand(0),
	mWriteBuffer(2048),
	mCbw(mBbaMem + CB_OFFSET, CB_SIZE)
{
	memset(mBbaMem, 0, BBA_MEM_SIZE);
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

	Expecting = EXPECT_NONE;
	mExpectVariableLengthImmWrite = false;
#ifdef FILEDEBUG
	ME = fopen("Debug.txt", "wb");
#endif
}

CEXIETHERNET::~CEXIETHERNET()
{
#ifdef FILEDEBUG
	fclose(ME);
#endif
}

void CEXIETHERNET::SetCS(int cs)
{
	DEBUGPRINT("Set CS: %s Expect Variable write?: %s", cs ? "true" : "false", mExpectVariableLengthImmWrite ? "true" : "false");
	if (!cs)
	{
		if (mExpectVariableLengthImmWrite)
		{
			mExpectVariableLengthImmWrite = false;
			mReadyToSend = true;
		}
		mExpectSpecialImmRead = false;
		mWriteP = mReadP = INVALID_P;
		m_bInterruptSet = false;
		//mBbaMem[BBA_IR] = 0;
		//mBbaMem[BBA_IRM] = 0;
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
	//bool Temp = m_bInterruptSet;
	//m_bInterruptSet = false;
	//return Temp;
	return m_bInterruptSet;
}

void CEXIETHERNET::recordSendComplete()
{
	mBbaMem[BBA_NCRA] &= ~0x06;
	if (mBbaMem[BBA_IMR] & BBA_INTERRUPT_SENT)
	{
		mBbaMem[BBA_IR] |= BBA_INTERRUPT_SENT;
		DEBUGPRINT("\t\tBBA Send interrupt raised");
		//exit(0);
		m_bInterruptSet = true;
		//interrupt.raiseEXI("BBA Send");
	}
	startRecv();
	mPacketsSent++;
}

bool CEXIETHERNET::checkRecvBuffer()
{
	if (mRecvBufferLength != 0)
	{
		handleRecvdPacket();
	}
	return true;
}

void CEXIETHERNET::ImmWrite(u32 _uData,  u32 _uSize)
{
	DEBUGPRINT("IMM Write, size 0x%x, data32: 0x%08x data16: 0x%04x data8: 0x%02x mWriteP 0x%x", _uSize, _uData, (u16)Common::swap32(_uData >> 8), (u8)Common::swap32(_uData), mWriteP);
	if (mExpectVariableLengthImmWrite) 
	{
		DEBUGPRINT("\t[INFO]Variable length IMM write");
		if (_uSize == 4)
		{
			// Correct
			_uData = Common::swap32(_uData);
		}
		else if (_uSize == 2)
		{
			// Correct
			_uData = (u16)Common::swap32(_uData);
		}
		mWriteBuffer.write(_uSize, &_uData);
		return;
	}
	else if (mWriteP != INVALID_P)
	{
		if (mWriteP + _uSize > BBA_MEM_SIZE)
		{
			DEBUGPRINT("[EEE]Write error: mWriteP + size = 0x%04X + %i", mWriteP, _uSize);
			exit(0);
		}
		DEBUGPRINT("\t[INFO]Write to BBA address 0x%0*X, %i byte%s: 0x%0*X",mWriteP >= CB_OFFSET ? 4 : 2, mWriteP, _uSize, (_uSize==1?"":"s"), _uSize*2, _uData);

		switch (mWriteP)
		{
		case BBA_IR:
			{
				// Correct, we use swapped
				_dbg_assert_(SP1, _uSize == 1);
				u32 SwappedData = Common::swap32(_uData);
				DEBUGPRINT("\t\t[INFO]BBA Interrupt reset 0x%02X & ~(0x%02X) => 0x%02X", mBbaMem[0x09], MAKE(u8, SwappedData), mBbaMem[0x09] & ~MAKE(u8, SwappedData));
				mBbaMem[BBA_IR] &= ~MAKE(u8, SwappedData);
				break;
			}
		case BBA_NCRA:
			{
				DEBUGPRINT("\t\t[INFO]BBA_NCRA");
				// Correct, we use the swap here
				u32 SwappedData = (u8)Common::swap32(_uData);
				if (RISE(BBA_NCRA_RESET)) 
				{
					// Normal
					// Whinecube did nothing else as well
					DEBUGPRINT("\t\t[INFO]BBA Reset");
				}
				if (RISE(BBA_NCRA_SR) && isActivated())
				{
					DEBUGPRINT("\t\t[INFO]BBA Start Receive");
					//exit(0);
					startRecv();
				}
				if (RISE(BBA_NCRA_ST1)) 
				{
					DEBUGPRINT("\t\t[INFO]BBA Start Transmit");
					if (!mReadyToSend)
					{
						DEBUGPRINT("\t\t\t[EEE]Not ready to send!");
						exit(0);
						//throw hardware_fatal_exception("BBA Transmit without a packet!");
					}
					sendPacket(mWriteBuffer.p(), mWriteBuffer.size());
					mReadyToSend = false;
				}
				mBbaMem[BBA_NCRA] = MAKE(u8, SwappedData);
			}
			break;
		case BBA_NWAYC:
			DEBUGPRINT("\t\t[INFO]BBA_NWAYC");
			if (Common::swap32(_uData) & (BBA_NWAYC_ANE | BBA_NWAYC_ANS_RA))
			{
				//say we've successfully negotiated for 10 Mbit full duplex
				//should placate libogc
				activate();
				mBbaMem[BBA_NWAYS] = (BBA_NWAYS_LS10 | BBA_NWAYS_LPNWAY | BBA_NWAYS_ANCLPT | BBA_NWAYS_10TXF);
			}
			else
			{
				if (_uData != 0x0)
				{
					DEBUGPRINT("Not activate!");
					exit(0);
				}
			}

			break;
		case BBA_RRP:	//RRP - Receive Buffer Read Page Pointer
			DEBUGPRINT("\t\t[INFO]RRP");
			_dbg_assert_(SP1, _uSize == 2 || _uSize == 1);
			mRBRPP = (u8)Common::swap32(_uData) << 8;	//Whinecube: I hope this works with both write sizes.
			mRBEmpty = (mRBRPP == ((u32)mCbw.p_write() + CB_OFFSET));
			checkRecvBuffer();
			break;
		case BBA_RWP:	//RWP - Receive Buffer Write Page Pointer
			DEBUGPRINT("\t\t[INFO]RWP");
			_dbg_assert_(SP1, _uSize == 2 || _uSize == 1);
			DEBUGPRINT("\t\t\tThing is 0x%0X", (u32)((u16)mCbw.p_write() + CB_OFFSET) >> 8);
			// TODO: This assert FAILS!
			//assert(Common::swap32(_uData) == (u32)((u16)mCbw.p_write() + CB_OFFSET) >> 8);
			break;
		case BBA_NWAYS:
			DEBUGPRINT("[ERR]Call to BBA_NWAYS directly!");
			exit(0);
			break;
		case BBA_SI_ACTRL2:
		default:
			DEBUGPRINT("\t\t[INFO]Default one!Size 0x%x _uData: 0x%08x Swapped 0x%08x to 0x%x", _uSize, _uData, Common::swap32(_uData),mWriteP);
			u32 SwappedData = 0;
			if (_uSize == 4 || _uSize == 1)
			{
				// Correct, use Swapped here
				// Size of 4 untested Though
				SwappedData = Common::swap32(_uData);
				if(_uSize == 4)
				{
					DEBUGPRINT("\t\t\tData is 0x%08x", SwappedData);
					//exit(0);
				}
			}
			else if ( _uSize == 2)
			{
				//Correct
				SwappedData = (u16)(_uData >> 16);
				//DEBUGPRINT("\t\t\tData is 0x%04x", SwappedData);
			}
			//u32 SwappedData = _uData;
			memcpy(mBbaMem + mWriteP, &SwappedData, _uSize);
			mWriteP = mWriteP + _uSize;
		}
		return;
	}
	else if (_uSize == 2 && _uData == 0)
	{
		// Device ID Request
		// 100% this returns correctly
		DEBUGPRINT("\t[INFO]Request Dev ID");
		mSpecialImmData = EXI_DEVTYPE_ETHER;
		mExpectSpecialImmRead = true;
		return;
	}
	else if ((_uSize == 4 && (_uData & 0xC0000000) == 0xC0000000) || (_uSize == 2 && ((u16)Common::swap32(_uData >> 8) & 0x4000) == 0x4000))
	{
		// Write to BBA Register
		DEBUGPRINT("\t[INFO]Write to BBA register!");
		if (_uSize == 4)
		{
			// Dunno if this is correct TODO
			u32 SwappedData = _uData;
			mWriteP = (u8)getbitsw(SwappedData, 16, 23);
		}
		else //size == 2
		{
			// Correct, Size of 1 untested, should be correct.
			u16 SwappedData = (u16)Common::swap32(_uData >> 8);
			mWriteP = (u8)getbitsw(SwappedData & ~0x4000, 16, 23);
		}
		//Write of size 4 data 0xc0006000 causes write pointer to be set to 0x0000 when swapped.
		// When not swapped, the write pointer is set to 0x0060
		if (mWriteP == 0x48)
		{
			mWriteBuffer.clear();
			mExpectVariableLengthImmWrite = true;
			DEBUGPRINT("\t\t[INFO]Prepared for variable length write to address 0x48");
		}
		else
		{
			DEBUGPRINT("\t\t[INFO]BBA Write pointer set to 0x%0*X", _uSize, mWriteP);
			//exit(0);
		}
		return;
	}
	else if ((_uSize == 4 && (_uData & 0xC0000000) == 0x80000000) || (_uSize == 2 && ((u16)Common::swap32(_uData >> 8) & 0x4000) == 0x0000))
	{	

		u32 SwappedData = _uData;
		// Read from BBA Register!
		if(_uSize == 4)
		{
			// Correct
			mReadP = (u16)getbitsw(SwappedData, 8, 23);
			if (mReadP >= BBA_MEM_SIZE)
			{
				DEBUGPRINT("\t\t[EEE]Illegal BBA address: 0x%04X", mReadP);
				//if(g::bouehr)
				exit(0);
				//return EXI_UNHANDLED;
			}
		}
		else
		{
			//Correct
			//size == 2
			mReadP = (u8)Common::swap32(_uData);
		}
		DEBUGPRINT("\t[INFO]Read from BBA register! 0x%X", mReadP);
		switch (mReadP)
		{
		case 0x20:	//MAC address
			DEBUGPRINT("\t\t[INFO]Mac Address!");
			memcpy(mBbaMem + mReadP, mac_address, 6);
			break;
		case 0x01:	//Revision ID
			break;
		case 0x16:	//RWP - Receive Buffer Write Page Pointer
			DEBUGPRINT("\t\t[INFO]RWP! 0x%04x Swapped 0x%04x", (((u16)mCbw.p_write() + CB_OFFSET) >> 8), Common::swap16(((u16)mCbw.p_write() + CB_OFFSET) >> 8));
			//exit(0);
			// TODO: Dunno if correct
			MAKE(u16, mBbaMem[mReadP]) = (((u16)mCbw.p_write() + CB_OFFSET) >> 8);
			break;
		case 0x18:	//RRP - Receive Buffer Read Page Pointer
			DEBUGPRINT("\t\t[INFO]RRP!");
			//exit(0);
			// TODO: Dunno if correct
			MAKE(u16, mBbaMem[mReadP]) = (mRBRPP) >> 8;
			break;
		case 0x3A:	//bit 1 set if no data available
			DEBUGPRINT("\t\t[INFO]Bit 1 set!");
			exit(0);
			//mBbaMem[mReadP] = !mRBEmpty;
			break;
		case 0x00:
			// These Two lines were commented out in Whinecube
			//mBbaMem[mReadP] = 0x00;
			//if(!sendInProgress())
			mBbaMem[mReadP] &= ~(0x06);
			//DEBUGPRINT("\t\t[INFO]mBbaMem[0x%x] &= ~(0x06);! Now 0x%x", mReadP, mBbaMem[mReadP]);
			//exit(0);
			break;
		case 0x03:
			mBbaMem[mReadP] = 0x80;
			//DEBUGPRINT("\t\t[INFO]mBbaMem[0x%x] = 0x80;! Now %x", mReadP, mBbaMem[mReadP]);
			//exit(0);
			break;
		case 0x0e:
		case 0x0f: // TWP - Transmit Buffer Write Page Pointer Register
			break;
		case 0x5b: // ??
		case 0x5c: // These two go together
			break;
		case 0x31: // NWAYS - NWAY Status Register
			// HACK
			activate();
			mBbaMem[BBA_NWAYS] = (BBA_NWAYS_LS10 | BBA_NWAYS_LPNWAY | BBA_NWAYS_ANCLPT | BBA_NWAYS_10TXF);
		case 0x09: // IR
		case 0x08: // IRM
		case 0x100: // Input Queue?
		case 0x110: // ?? Not even in YAGCD
		case 0x04: // LTPS - Last Transmitted Packet Status (transmit error code ?)
		case 0x30: // NWAYC - NWAY Configuration Register
			break;
		default:
			DEBUGPRINT("Read from 0x%02x", mReadP);
			//exit(0);
			break;
		}
		//DEBUGPRINT("BBA Read pointer set to 0x%0*X, Data: 0x%08X", _uSize, mReadP, _uData);
		return;
	}
	DEBUGPRINT("\t[EEE]Not expecting ImmWrite of size %d", _uSize);
	DEBUGPRINT("\t\t[INFO] SKIPPING!");
	//exit(0);
}

u32 CEXIETHERNET::ImmRead(u32 _uSize)
{
	DEBUGPRINT("IMM Read, size 0x%x", _uSize);
	if (mExpectSpecialImmRead)
	{
		// 100% that this returns correctly
		DEBUGPRINT("\t[INFO]special IMMRead");
		mExpectSpecialImmRead = false;
		return mSpecialImmData;
	}
	if (mReadP != INVALID_P)
	{
		if (mReadP + _uSize > BBA_MEM_SIZE)
		{
			DEBUGPRINT("\t[EEE]Read error: mReadP + size = 0x%04X + %i", mReadP, _uSize);
			exit(0);
		}
		u32 uResult = 0;
		memcpy(&uResult, mBbaMem + mReadP, _uSize);
		// TODO: We do as well?
		uResult = Common::swap32(uResult); //Whinecube : we have a byteswap problem...

		//DEBUGPRINT("Mem spot is 0x%02x uResult is 0x%x", mBbaMem[mReadP], uResult);
		/*#ifndef _WIN32
		if(CheckRecieved())
		startRecv();
		#endif*/
		DEBUGPRINT("\t[INFO]Read from BBA address 0x%0*X, %i byte%s: 0x%0*X",mReadP >= CB_OFFSET ? 4 : 2, mReadP, _uSize, (_uSize==1?"":"s"),_uSize*2, getbitsw(uResult, 0, _uSize * 8 - 1));
		mReadP = mReadP + _uSize;
		return uResult;
	}
	else
	{
		DEBUGPRINT("\t[EEE]Unhandled IMM read of %d bytes", _uSize);
		exit(0);
	}
	DEBUGPRINT("[EEE]Not Expecting IMMRead of size %d!", _uSize);
	exit(0);
}

void CEXIETHERNET::DMAWrite(u32 _uAddr, u32 _uSize)
{
	if (mExpectVariableLengthImmWrite)
	{
		DEBUGPRINT("DMA Write: Address is 0x%x and size is 0x%x", _uAddr, _uSize);
		mWriteBuffer.write(_uSize, Memory::GetPointer(_uAddr));
		return;
	}
	else
	{
		DEBUGPRINT("Unhandled BBA DMA write: %i, 0x%08X", _uSize, _uAddr);
		//if(g::bouehr)
		exit(0);
		//return EXI_UNHANDLED;
	}
}

void CEXIETHERNET::DMARead(u32 _uAddr, u32 _uSize)
{
	if (mReadP != INVALID_P)
	{
		if (mReadP + _uSize > BBA_MEM_SIZE)
		{
			DEBUGPRINT("Read error: mReadP + size = 0x%04X + %i", mReadP, _uSize);
			return;
		}
		//mem.write_physical(address, size, mBbaMem + mReadP);
		memcpy(Memory::GetPointer(_uAddr), mBbaMem + mReadP, _uSize);
		DEBUGPRINT("DMA Read from BBA address 0x%0*X, %i bytes",
			mReadP >= CB_OFFSET ? 4 : 2, mReadP, _uSize);
		//exit(0);
		mReadP = mReadP + _uSize;
		return;
	}
	else
	{
		DEBUGPRINT("Unhandled BBA DMA read: %i, 0x%08X", _uSize, _uAddr);
		//if(g::bouehr)
		exit(0);
		//throw bouehr_exception("Unhandled BBA DMA read");
		//return EXI_UNHANDLED;
	}
}
