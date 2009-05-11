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

#include "Memmap.h"

#include <assert.h>
#include "../Core.h"

#include "EXI_Device.h"
#include "EXI_DeviceEthernet.h"

//#define SONICDEBUG

void DEBUGPRINT (const char * format, ...)
{
	char buffer[256];
	va_list args;
	va_start (args, format);
	vsprintf (buffer,format, args);
	#ifdef SONICDEBUG
		printf("%s", buffer);
	#else
		INFO_LOG(SP1, buffer);
	#endif
	va_end (args);
}
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



#define MAKE(type, arg) (*(type *)&(arg))

#define RISE(flags) ((SwappedData & (flags)) && !(mBbaMem[0x00] & (flags)))

int mPacketsSent = 0;
u8 mac_address[6] = {0x4D, 0xFF, 0x11, 0x88, 0xF1, 0x76};
unsigned int Expecting;

CEXIETHERNET::CEXIETHERNET() :
	m_uPosition(0),
	m_uCommand(0),
	mWriteBuffer(2048),
	mCbw(mBbaMem + CB_OFFSET, CB_SIZE)
{
	ID = 0x04020200;
	mWriteP = INVALID_P;
	mReadP = INVALID_P;
	mReadyToSend = false;
	Activated = false;
	
	mRecvBufferLength = 0;
	
	mExpectSpecialImmRead = false;
	
	Expecting = EXPECT_NONE;
	mExpectVariableLengthImmWrite = false;
}

void CEXIETHERNET::SetCS(int cs)
{
	if (cs)
	{
		if (mExpectVariableLengthImmWrite)
		{
			mExpectVariableLengthImmWrite = false;
			mReadyToSend = true;
		}
		mWriteP = mReadP = INVALID_P;
		mExpectSpecialImmRead = false;
		m_uPosition = 0;
		Expecting = EXPECT_NONE;
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
	return false;
}

void CEXIETHERNET::recordSendComplete() 
{
	mBbaMem[0x00] &= ~0x06;
	if(mBbaMem[0x08] & BBA_INTERRUPT_SENT) 
	{
		mBbaMem[0x09] |= BBA_INTERRUPT_SENT;
		DEBUGPRINT( "BBA Send interrupt raised\n");
		exit(0);
		//interrupt.raiseEXI("BBA Send");
	}
	mPacketsSent++;
}

bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	DEBUGPRINT( "Packet: 0x");
	for(int a = 0; a < size; ++a)
	{
		DEBUGPRINT( "%02X", etherpckt[a]);
	}
	DEBUGPRINT( " : Size: %d\n", size);
	/*DWORD numBytesWrit;
	OVERLAPPED overlap;
	ZERO_OBJECT(overlap);
	//overlap.hEvent = mHRecvEvent;
	TGLE(WriteFile(mHAdapter, etherpckt, size, &numBytesWrit, &overlap));
	if(numBytesWrit != size) 
	{
		DEGUB("BBA sendPacket %i only got %i bytes sent!\n", size, numBytesWrit);
		FAIL(UE_BBA_ERROR);
	}*/
	recordSendComplete();
	//exit(0);
	return true;
}
bool CEXIETHERNET::handleRecvdPacket() 
{
	DEBUGPRINT(" Handle received Packet!\n");
	exit(0);
}
bool CEXIETHERNET::checkRecvBuffer() 
{
	if(mRecvBufferLength != 0) 
	{
		handleRecvdPacket();
	}
	return true;
}
void CEXIETHERNET::ImmWrite(u32 _uData,  u32 _uSize)
{
	DEBUGPRINT( "IMM Write, size 0x%x, data 0x%x mWriteP 0x%x\n", _uSize, _uData, mWriteP);
	if (mExpectVariableLengthImmWrite) 
	{
		DEBUGPRINT( "Not doing expecting variable length imm write!\n");
		exit(0);
	}
	else if (mWriteP != INVALID_P) 
	{
		if (mWriteP + _uSize > BBAMEM_SIZE) 
		{
			DEBUGPRINT( "[EEE]Write error: mWriteP + size = 0x%04X + %i\n", mWriteP, _uSize);
			exit(0);
		}
		//BBADEGUB("Write to BBA address 0x%0*X, %i byte%s: 0x%0*X\n",mWriteP >= CB_OFFSET ? 4 : 2, mWriteP, size, (size==1?"":"s"), size*2, data);

		switch (mWriteP) 
		{
			case 0x09:
			{
				//BBADEGUB("BBA Interrupt reset 0x%02X & ~(0x%02X) => 0x%02X\n", mBbaMem[0x09], MAKE(BYTE, data), mBbaMem[0x09] & ~MAKE(BYTE, data));
				//assert(_uSize == 1);
				// TODO: Should we swap our data?
				// With _uData not swapped, it becomes 0 when the data is 0xff000000
				// With _uData swapped, it becomes 0 as well. Who knows the right way?
				u32 SwappedData = Common::swap32(_uData);
				mBbaMem[0x09] &= ~MAKE(u8, SwappedData);
				DEBUGPRINT( "\t[INFO]mWriteP is %x. mBbaMem[0x09] is 0x%x\n", mWriteP, mBbaMem[0x09]);
				//exit(0);
				break;
			}
			case BBA_NCRA:
			{
				u32 SwappedData = Common::swap32(_uData);
				//u32 SwappedData = _uData;
				// TODO: Should we swap our data?
				if (RISE(BBA_NCRA_RESET)) 
				{
					// Normal
					// Whinecube did nothing else as well
					DEBUGPRINT( "\t[INFO]BBA Reset\n");
				}
				if (RISE(BBA_NCRA_SR) && isActivated())
				{
					DEBUGPRINT( "\t[INFO]BBA Start Recieve\n");
					exit(0);
					// TODO: Need to make our virtual network device start receiving
					//HWGLE(startRecv());
				}
				if (RISE(BBA_NCRA_ST1)) 
				{
					DEBUGPRINT( "\t[INFO]BBA Start Transmit\n");
					if (!mReadyToSend)
					{
						DEBUGPRINT( "\t\t[EEE]Not ready to send!\n");
						exit(0);
						//throw hardware_fatal_exception("BBA Transmit without a packet!");
					}
					// TODO: Actually Make it send a packet
					sendPacket(mWriteBuffer.p(), mWriteBuffer.size());
					mReadyToSend = false;
					//exit(0);
				}
				mBbaMem[0x00] = MAKE(u8, SwappedData);
			}
				break;
			case BBA_NWAYC:
				DEBUGPRINT( "\t[INFO]BBA_NWAYCn");
				if(_uData & (BBA_NWAYC_ANE | BBA_NWAYC_ANS_RA)) 
				{
					activate();
					//say we've successfully negotiated for 10 Mbit full duplex
					//should placate libogc
					mBbaMem[BBA_NWAYS] = BBA_NWAYS_LS10 | BBA_NWAYS_LPNWAY |
						BBA_NWAYS_ANCLPT | BBA_NWAYS_10TXF;
				}
				break;
			case 0x18:	//RRP - Receive Buffer Read Page Pointer
				DEBUGPRINT( "\t[INFO]RRP\n");
				//exit(0);
				assert(_uSize == 2 || _uSize == 1);
				mRBRPP = (u8)_uData << 8;	//Whinecube: I hope this works with both write sizes.
				mRBEmpty = mRBRPP == ((u32)mCbw.p_write() + CB_OFFSET);
				checkRecvBuffer();
				break;
			case 0x16:	//RWP - Receive Buffer Write Page Pointer
				DEBUGPRINT( "\t[INFO]RWP\n");
				//exit(0);
				/*MYASSERT(size == 2 || size == 1);
				MYASSERT(data == DWORD((WORD)mCbw.p_write() + CB_OFFSET) >> 8);*/
				break;
			default:
				DEBUGPRINT( "\t[INFO]Default one!Size 0x%x _uData: 0x%08x Swapped 0x%08x to 0x%x\n", _uSize, _uData, Common::swap32(_uData),mWriteP);
				//u32 SwappedData = Common::swap32(_uData);
				u32 SwappedData = _uData;
				memcpy(mBbaMem + mWriteP, &SwappedData, _uSize);
				mWriteP = mWriteP + _uSize;
		}
		return;
	}
	else if (_uSize == 2 && _uData == 0) 
	{
		// Device ID Request
		// 100% this returns correctly
		DEBUGPRINT( "\t[INFO]Request Dev ID\n");
		mSpecialImmData = EXI_DEVTYPE_ETHER;
		mExpectSpecialImmRead = true;
		return;
	}
	else if ((_uSize == 4 && (_uData & 0xC0000000) == 0xC0000000) || (_uSize == 2 && (_uData & 0x4000) == 0x4000))
	{
		// Write to BBA Register
		DEBUGPRINT( "\t[INFO]Write to BBA register!\n");
		//u32 SwappedData = Common::swap32(_uData);
		u32 SwappedData = _uData;
		if (_uSize == 4)
			mWriteP = (u8)getbitsw(SwappedData, 16, 23);
		else  //size == 2
			mWriteP = (u8)getbitsw(SwappedData & ~0x4000, 16, 23);  //Whinecube : Dunno about this...
		//Write of size 4 data 0xc0006000 causes write pointer to be set to 0x0000 when swapped.
		// When not swapped, the write pointer is set to 0x0060
		if (mWriteP == 0x48) 
		{
			mWriteBuffer.clear();
			mExpectVariableLengthImmWrite = true;
			DEBUGPRINT( "\t\t[INFO]Prepared for variable length write to address 0x48\n");
		} 
		else 
		{
			DEBUGPRINT( "\t\t[INFO]BBA Write pointer set to 0x%0*X\n", _uSize, mWriteP);
			//exit(0);
		}
		return;
	}
	else if ((_uSize == 4 && (_uData & 0xC0000000) == 0x80000000) || (_uSize == 2 && (_uData & 0x4000) == 0x0000))
	{	
		DEBUGPRINT( "\t[INFO]Read from BBA register!\n");
		// If swapped, we get a read from invalid BBA Address 0x2000 in Mario Kart: DD
		// If not swapped, we always end up with an unexpected IMMwrite of 1 byte
		//u32 SwappedData = Common::swap32(_uData);
		u32 SwappedData = _uData;
		// Read from BBA Register!
		if(_uSize == 4) 
		{
			mReadP = (u32)getbitsw(SwappedData, 8, 23);
			if (mReadP >= BBAMEM_SIZE)
			{
				DEBUGPRINT( "\t\t[EEE]Illegal BBA address: 0x%04X\n", mReadP);
				//if(g::bouehr)
				exit(0);
				//return EXI_UNHANDLED;
			}
		} 
		else 
		{  //size == 2
			mReadP = (u8)getbitsw(SwappedData, 16, 23);
		}
		// With the data not swapped,after a few reads, nReadP is always 0 in Mario Kart: DD; Size always 2
		// Before that, it does request the MAC address if it's unswapped
		switch (mReadP) 
		{
		case 0x20:	//MAC address
			DEBUGPRINT( "\t\t[INFO]Mac Address!\n");
			memcpy(mBbaMem + mReadP, mac_address, 6);
			break;
		case 0x01:	//Revision ID
			break;
		case 0x16:	//RWP - Receive Buffer Write Page Pointer
			DEBUGPRINT( "\t\t[INFO]RWP!\n");
			exit(0);
			//MAKE(WORD, mBbaMem[mReadP]) = ((WORD)mCbw.p_write() + CB_OFFSET) >> 8;
			break;
		case 0x18:	//RRP - Receive Buffer Read Page Pointer
			DEBUGPRINT( "\t\t[INFO]RRP!\n");
			exit(0);
			//MAKE(WORD, mBbaMem[mReadP]) = (mRBRPP) >> 8;
			break;
		case 0x3A:	//bit 1 set if no data available
			DEBUGPRINT( "\t\t[INFO]Bit 1 set!\n");
			exit(0);
			//mBbaMem[mReadP] = !mRBEmpty;
			break;
		case 0x00:
			// These Two lines were commented out in Whinecube
			//mBbaMem[mReadP] = 0x00;
			//if(!sendInProgress())
			mBbaMem[mReadP] &= ~(0x06);
			DEBUGPRINT( "\t\t[INFO]mBbaMem[0x%x] &= ~(0x06);! Now 0x%x\n", mReadP, mBbaMem[mReadP]);
			//exit(0);
			break;
		case 0x03:
			mBbaMem[mReadP] = 0x80;
			DEBUGPRINT( "\t\t[INFO]mBbaMem[0x%x] = 0x80;! Now %x\n", mReadP, mBbaMem[mReadP]);
			exit(0);
			break;
		}
		printf("\t\t[INFO]BBA Read pointer set to 0x%0*X\n", _uSize, mReadP);
		return;
	}
	DEBUGPRINT( "\t[EEE]Not expecting ImmWrite of size %d\n", _uSize);
	DEBUGPRINT( "\t\t[INFO] SKIPPING!\n");
	//exit(0);
}

u32 CEXIETHERNET::ImmRead(u32 _uSize)
{
	DEBUGPRINT( "IMM Read, size 0x%x\n", _uSize);
	if (mExpectSpecialImmRead) 
	{
		// 100% that this returns correctly
		DEBUGPRINT( "\t[INFO]special IMMRead\n");
		mExpectSpecialImmRead = false;
		return mSpecialImmData;
	}
	if (mReadP != INVALID_P) 
	{
		if (mReadP + _uSize > BBAMEM_SIZE) 
		{
			DEBUGPRINT( "\t[EEE]Read error: mReadP + size = 0x%04X + %i\n", mReadP, _uSize);
			exit(0);
		}
		u32 uResult = 0;
		memcpy(&uResult, mBbaMem + mReadP, _uSize);
		// TODO: We do as well?
		
		uResult = Common::swap32(uResult); //Whinecube : we have a byteswap problem...
		DEBUGPRINT( "\t[INFO]Read from BBA address 0x%0*X, %i byte%s: 0x%0*X\n",mReadP >= CB_OFFSET ? 4 : 2, mReadP, _uSize, (_uSize==1?"":"s"),_uSize*2, getbitsw(uResult, 0, _uSize * 8 - 1));
		mReadP = mReadP + _uSize;
		return uResult;
	}
	else
	{
		DEBUGPRINT( "\t[EEE]Unhandled IMM read of %d bytes\n", _uSize);
	}
	DEBUGPRINT( "[EEE]Not Expecting IMMRead of size %d!\n", _uSize);
	exit(0);
}

void CEXIETHERNET::DMAWrite(u32 _uAddr, u32 _uSize)
{
	DEBUGPRINT( "DMAW\n");
	exit(0);
}

void CEXIETHERNET::DMARead(u32 _uAddr, u32 _uSize) 
{
	DEBUGPRINT( "DMAR\n");
	exit(0);
};
