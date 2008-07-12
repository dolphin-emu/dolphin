#pragma once
#include "../plugin_specs/video.h"
#include "structs.h"

struct ReaderInterface
{
	_u8  (*Read8) (void);
	_u8  (*Peek8) (void); //to combine primitive draws.. 
	_u16 (*Read16)(void);
	_u32 (*Read32)(void);
	_u32 (*GetPtr)(void);
};

extern ReaderInterface *reader;
extern ReaderInterface fifoReader,dlistReader,bufReader;

namespace OpcodeReaders
{
	void SetDListReader(_u32 _ptr, _u32 _end);
	void DListReaderSkip(int _skip);
	void SetMemPtr(_u8 *_mptr);
	void SetFifoData(FifoData *_fdata);
	bool IsDListOKToRead();

	void SetBufPtr(_u8 *_bufptr);
}
