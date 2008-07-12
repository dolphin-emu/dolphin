#include "OpcodeReaders.h"

_u8 *readerMemPtr;
_u8 *bufPtr;
FifoData *gFifoData;
_u32 gDListEnd, gDListPtr;

namespace OpcodeReaders
{
	void SetBufPtr(_u8 *_bufptr)
	{
		bufPtr = _bufptr;
	}
	void SetFifoData(FifoData *_fdata)
	{
		gFifoData = _fdata;
	}
	void SetMemPtr(_u8 *_mptr)
	{
		readerMemPtr = _mptr;
	}

	void SetDListReader(_u32 _ptr, _u32 _end)
	{
		gDListPtr = _ptr & 0x1FFFFFF;
		gDListEnd = _end & 0x1FFFFFF;
	}
	void DListReaderSkip(int _skip)
	{
		gDListPtr+=_skip;
	}
	bool IsDListOKToRead()
	{
		return gDListPtr<gDListEnd;
	}
}
//FifoData *fifoData;

_u8 PeekFifo8()
{
	int addr = (gFifoData->readptr+1)^3;
	if (addr==gFifoData->gpend)
		addr=gFifoData->gpbegin;
	return readerMemPtr[addr];
}
// ________________________________________________________________________________________________
// ReadFifo8
//
_u8 ReadFifo8()
{
	while (
		(gFifoData->readenable == false) || 
		(gFifoData->readptr == gFifoData->writeptr) || 
		(gFifoData->bpenable && (gFifoData->readptr == gFifoData->breakpt))
		)
	{
		//if (gFifoData->readptr == gFifoData->breakpt)
		//	MessageBox(0,"hello breakpoint",0,0);
		SwitchToFiber(gFifoData->cpuFiber);
	}

	_u8 val = readerMemPtr[(gFifoData->readptr++)^3];

	if (gFifoData->readptr == gFifoData->gpend)
		gFifoData->readptr = gFifoData->gpbegin;

	return val;
}

// ________________________________________________________________________________________________
// ReadFifo16
//
_u16 ReadFifo16()
{
	//PowerPC byte ordering :(
	_u8 val1 = ReadFifo8();
	_u8 val2 = ReadFifo8();
	return (val1<<8)|(val2);
}

// ________________________________________________________________________________________________
// ReadFifo32
//
_u32 ReadFifo32()
{
	//PowerPC byte ordering :(
	_u8 val1 = ReadFifo8();
	_u8 val2 = ReadFifo8();
	_u8 val3 = ReadFifo8();
	_u8 val4 = ReadFifo8();
	return (val1<<24)|(val2<<16)|(val3<<8)|(val4);
}

_u32 GetPtrFifo()
{
	return gFifoData->readptr;
}

_u8 PeekDList8()
{
	if (gDListPtr<gDListEnd-1)
		return readerMemPtr[(gDListPtr+1)^3];
	else
		return 0;
}
// ________________________________________________________________________________________________
// ReadFifo8
//
_u8 ReadDList8()
{
	return readerMemPtr[(gDListPtr++)^3];
}

// ________________________________________________________________________________________________
// ReadFifo16
//
_u16 ReadDList16()
{
	//PowerPC byte ordering :(
	_u8 val1 = readerMemPtr[(gDListPtr++)^3];
	_u8 val2 = readerMemPtr[(gDListPtr++)^3];
	return (val1<<8)|(val2);
}

// ________________________________________________________________________________________________
// ReadFifo32
//
_u32 ReadDList32()
{
	//PowerPC byte ordering :(
	_u8 val1 = readerMemPtr[(gDListPtr++)^3];
	_u8 val2 = readerMemPtr[(gDListPtr++)^3];
	_u8 val3 = readerMemPtr[(gDListPtr++)^3];
	_u8 val4 = readerMemPtr[(gDListPtr++)^3];
	return (val1<<24)|(val2<<16)|(val3<<8)|(val4);
}

_u32 GetPtrDList()
{
	return gDListPtr;
}


_u8 PeekBuf8()
{
	return bufPtr[0];
}

// ________________________________________________________________________________________________
// ReadFifo8
//
_u8 ReadBuf8()
{
	return *bufPtr++;
}

// ________________________________________________________________________________________________
// ReadFifo16
//
_u16 ReadBuf16()
{
	_u16 val = *(_u16*)bufPtr;
	bufPtr+=2;
	return (val<<8)|(val>>8);
}

// ________________________________________________________________________________________________
// ReadFifo32
//
_u32 ReadBuf32()
{
//	_u32 val = *(_u32*)bufPtr;
	//__asm
//	{
//		mov ebx,bufPtr
//		mov eax,[ebx]
//		add ebx,4
//		mov bufPtr,ebx
//		bswap eax
//	}
	_u32 high = ReadBuf16();
	return (high<<16) | ReadBuf16();
//	return swap32(val);
}


ReaderInterface fifoReader =
{
	ReadFifo8,
	PeekFifo8,
	ReadFifo16,
	ReadFifo32,
	GetPtrFifo
};


ReaderInterface dlistReader = 
{
	ReadDList8,
	PeekDList8,
	ReadDList16,
	ReadDList32,
	GetPtrDList
};

ReaderInterface bufReader = 
{
	ReadBuf8,
	PeekBuf8,
	ReadBuf16,
	ReadBuf32,
	0
};

ReaderInterface *reader;
