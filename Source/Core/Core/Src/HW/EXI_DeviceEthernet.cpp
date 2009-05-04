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

#include "../Core.h"

#include "EXI_Device.h"
#include "EXI_DeviceEthernet.h"
enum {
	EXPECT_NONE = 0,
	EXPECT_ID,
} ;
#define BBA_INTERRUPT_RECV 0x02
#define BBA_INTERRUPT_SENT 0x04
#define BBA_INTERRUPT_RECV_ERROR 0x08
#define BBA_INTERRUPT_SEND_ERROR 0x10
int mPacketsSent = 0;
u8 mac_address[6] = {0x4D, 0xFF, 0x11, 0x88, 0xF1, 0x76};
unsigned int Expecting;
CEXIETHERNET::CEXIETHERNET() :
	m_uPosition(0),
	m_uCommand(0),
	mWriteBuffer(2048)
{
	ID = 0x04020200;
	mWriteP = INVALID_P;
	mReadP = INVALID_P;
	mReadyToSend = false;
	Activated = false;
	
	mExpectSpecialImmRead = false;
	
	Expecting = EXPECT_NONE;
	mExpectVariableLengthImmWrite = false;
}

void CEXIETHERNET::SetCS(int cs)
{
	if (cs)
	{
		if(mExpectVariableLengthImmWrite)
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
bool CEXIETHERNET::isActivated()
{
	// Todo: Return actual check
	return Activated;
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

void CEXIETHERNET::recordSendComplete() 
{
	mBbaMem[0x00] &= ~0x06;
	if(mBbaMem[0x08] & BBA_INTERRUPT_SENT) 
	{
		mBbaMem[0x09] |= BBA_INTERRUPT_SENT;
		printf("BBA Send interrupt raised\n");
		exit(0);
		//interrupt.raiseEXI("BBA Send");
	}
	mPacketsSent++;
}

bool CEXIETHERNET::sendPacket(u8 *etherpckt, size_t size) 
{
		printf("Packet: 0x");
		for(int a = 0; a < size; ++a)
		{
			printf("%02X", etherpckt[a]);
		}
		printf(" : Size: %d\n", size);
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

void CEXIETHERNET::ImmWrite(u32 _uData,  u32 _uSize)
{
	printf("IMM Write, size 0x%x, data 0x%x mWriteP 0x%x\n", _uSize, _uData, mWriteP);
	if(mExpectVariableLengthImmWrite) 
	{
		printf("Not doing expecting variable length imm write!\n");
		exit(0);
	}
	else if(mWriteP != INVALID_P) 
	{
		if(mWriteP + _uSize > BBAMEM_SIZE) 
		{
			printf("Write error: mWriteP + size = 0x%04X + %i\n", mWriteP, _uSize);
			exit(0);
		}
		//BBADEGUB("Write to BBA address 0x%0*X, %i byte%s: 0x%0*X\n",mWriteP >= CB_OFFSET ? 4 : 2, mWriteP, size, (size==1?"":"s"), size*2, data);

		switch(mWriteP) 
		{
			case 0x09:
				printf("mWriteP is %x\n", mWriteP);
				//BBADEGUB("BBA Interrupt reset 0x%02X & ~(0x%02X) => 0x%02X\n", mBbaMem[0x09], MAKE(BYTE, data), mBbaMem[0x09] & ~MAKE(BYTE, data));
				//assert(_uSize == 1);
				// TODO: Should we swap our data?
				mBbaMem[0x09] &= ~MAKE(u8, _uData);
				exit(0);
				break;
			case BBA_NCRA:
			{
				u32 SwappedData = Common::swap32(_uData);
				// TODO: Should we swap our data?
				#define RISE(flags) ((SwappedData & (flags)) && !(mBbaMem[0x00] & (flags)))
				if(RISE(BBA_NCRA_RESET)) 
				{
					// Normal
					// Whinecube did nothing else as well
					printf("BBA Reset\n");
				}
				if(RISE(BBA_NCRA_SR) && isActivated())
				{
					printf("BBA Start Recieve\n");
					exit(0);
					// TODO: Need to make our virtual network device start receiving
					//HWGLE(startRecv());
				}
				if(RISE(BBA_NCRA_ST1)) 
				{
					printf("BBA Start Transmit\n");
					if(!mReadyToSend)
					{
						printf("Not ready to send!\n");
						exit(0);
						//throw hardware_fatal_exception("BBA Transmit without a packet!");
					}
					// TODO: Actually Make it send a packet
					sendPacket(mWriteBuffer.p(), mWriteBuffer.size());
					mReadyToSend = false;
					//exit(0);
				}
				mBbaMem[0x00] = MAKE(u8, _uData);
			}
				break;
			case BBA_NWAYC:
				printf("mWriteP is %x\n", mWriteP);
				exit(0);
				/*if(data & (BBA_NWAYC_ANE | BBA_NWAYC_ANS_RA)) 
				{
					HWGLE(activate());
					//say we've successfully negotiated for 10 Mbit full duplex
					//should placate libogc
					mBbaMem[BBA_NWAYS] = BBA_NWAYS_LS10 | BBA_NWAYS_LPNWAY |
						BBA_NWAYS_ANCLPT | BBA_NWAYS_10TXF;
				}*/
				break;
			case 0x18:	//RRP - Receive Buffer Read Page Pointer
				printf("mWriteP is %x\n", mWriteP);
				exit(0);
				/*MYASSERT(size == 2 || size == 1);
				mRBRPP = (BYTE)data << 8;	//I hope this works with both write sizes.
				mRBEmpty = mRBRPP == ((WORD)mCbw.p_write() + CB_OFFSET);
				HWGLE(checkRecvBuffer());*/
				break;
			case 0x16:	//RWP
				printf("mWriteP is %x\n", mWriteP);
				exit(0);
				/*MYASSERT(size == 2 || size == 1);
				MYASSERT(data == DWORD((WORD)mCbw.p_write() + CB_OFFSET) >> 8);*/
				break;
			default:
				printf("Default one!Size 0x%x _uData: 0x%08x Swapped 0x%08x to 0x%x\n", _uSize, _uData, Common::swap32(_uData),mWriteP);
				u32 SwappedData = Common::swap32(_uData);
				memcpy(mBbaMem + mWriteP, &SwappedData, _uSize);
				mWriteP = mWriteP + _uSize;
		}
		return;
	}else if(_uSize == 2 && _uData == 0) 
	{
		// Device ID Request
		// 100% this returns correctly
		mSpecialImmData = EXI_DEVTYPE_ETHER;
		mExpectSpecialImmRead = true;
		return;
	}
	else if((_uSize == 4 && (_uData & 0xC0000000) == 0xC0000000) || (_uSize == 2 && (_uData & 0x4000) == 0x4000))
	{ // Write to BBA Register
		printf("Write to BBA register!\n");
		u32 SwappedData = Common::swap32(_uData);
		//u32 SwappedData = _uData;
		if(_uSize == 4)
			mWriteP = (u8)getbitsw(SwappedData, 16, 23);
		else  //size == 2
			mWriteP = (u8)getbitsw(SwappedData & ~0x4000, 16, 23);  //Whinecube : Dunno about this...
		// With Size of 4 and data of 0xc0006000 unswapped
		// mWriteP will be 0x0060 unswapped and 0x0000 swapped. Which is correct?
		if(mWriteP == 0x48) 
		{
			mWriteBuffer.clear();
			mExpectVariableLengthImmWrite = true;
			printf("Prepared for variable length write to address 0x48\n");
		} 
		else 
		{
			
			printf("BBA Write pointer set to 0x%0*X\n", _uSize, mWriteP);
			//exit(0);
		}
		return;
	}
	else if((_uSize == 4 && (_uData & 0xC0000000) == 0x80000000) || (_uSize == 2 && (_uData & 0x4000) == 0x0000))
	{	
		printf("Read from BBA register!\n");
		//u32 SwappedData = Common::swap32(_uData);
		u32 SwappedData = _uData;
		// Read from BBA Register!
		if(_uSize == 4) 
		{
			mReadP = (u32)getbitsw(SwappedData, 8, 23);
			if(mReadP >= BBAMEM_SIZE)
			{
				printf("Illegal BBA address: 0x%04X\n", mReadP);
				//if(g::bouehr)
				exit(0);
				//return EXI_UNHANDLED;
			}
		} 
		else 
		{  //size == 2
			mReadP = (u8)getbitsw(SwappedData, 16, 23);
		}
		switch(mReadP) 
		{
			case 0x20:	//MAC address
				printf("Mac Address!\n");
				memcpy(mBbaMem + mReadP, mac_address, 6);
				break;
			case 0x01:	//Revision ID
				break;
			case 0x16:	//RWP - Receive Buffer Write Page Pointer
				printf("RWP!\n");
				exit(0);
				//MAKE(WORD, mBbaMem[mReadP]) = ((WORD)mCbw.p_write() + CB_OFFSET) >> 8;
				break;
			case 0x18:	//RRP - Receive Buffer Read Page Pointer
				printf("RRP!\n");
				exit(0);
				//MAKE(WORD, mBbaMem[mReadP]) = (mRBRPP) >> 8;
				break;
			case 0x3A:	//bit 1 set if no data available
				printf("Bit 1 set!\n");
				exit(0);
				//mBbaMem[mReadP] = !mRBEmpty;
				break;
			case 0x00:
				// These Two lines were commented out in Whinecube
				//mBbaMem[mReadP] = 0x00;
				//if(!sendInProgress())
				mBbaMem[mReadP] &= ~(0x06);
				printf("mBbaMem[0x%x] &= ~(0x06);! Now %x\n", mReadP, mBbaMem[mReadP]);
				//exit(0);
				break;
			case 0x03:
				mBbaMem[mReadP] = 0x80;
				printf("mBbaMem[0x%x] = 0x80;! Now %x\n", mReadP, mBbaMem[mReadP]);
				exit(0);
				break;
		}
		//BBADEGUB("BBA Read pointer set to 0x%0*X\n", size, mReadP);
		return;
	}
	printf("Not expecting ImmWrite of size %d\n", _uSize);
	exit(0);
}
u32 CEXIETHERNET::ImmRead(u32 _uSize)
{
	printf("IMM Read, size 0x%x\n", _uSize);
	if(mExpectSpecialImmRead) 
	{
		// 100% that this returns correctly
		printf("special IMMRead\n");
		mExpectSpecialImmRead = false;
		return mSpecialImmData;
	}
	if(mReadP != INVALID_P) 
	{
		if(mReadP + _uSize > BBAMEM_SIZE) 
		{
			printf("Read error: mReadP + size = 0x%04X + %i\n", mReadP, _uSize);
			exit(0);
		}
		u32 uResult = 0;
		memcpy(&uResult, mBbaMem + mReadP, _uSize);
		// TODO: We do as well?
		
		//uResult = Common::swap32(uResult); //Whinecube : we have a byteswap problem...
		printf("Read from BBA address 0x%0*X, %i byte%s: 0x%0*X\n",mReadP >= CB_OFFSET ? 4 : 2, mReadP, _uSize, (_uSize==1?"":"s"),_uSize*2, getbitsw(uResult, 0, _uSize * 8 - 1));
		mReadP = mReadP + _uSize;
		return uResult;
	}
	else
	{
		printf("Unhandled IMM read of %d bytes\n", _uSize);
	}
	printf("Not Expecting IMMRead of size %d!\n", _uSize);
	exit(0);
}

void CEXIETHERNET::DMAWrite(u32 _uAddr, u32 _uSize)
{
	printf("DMAW\n");
	exit(0);
}

void CEXIETHERNET::DMARead(u32 _uAddr, u32 _uSize) 
{
	printf("DMAR\n");
	exit(0);
};
