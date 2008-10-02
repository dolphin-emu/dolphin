// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _DATAREADER_H
#define _DATAREADER_H

#include "Fifo.h"


#if !defined(DATAREADER_INLINE) || defined(DATAREADER_DEBUG)
// =================================================================================================
// IDataReader
// =================================================================================================

class IDataReader
{
protected:
	virtual ~IDataReader();

public:	
    virtual void Skip(u32) = 0;
    virtual u8  Read8 () = 0;
    virtual u16 Read16() = 0;
    virtual u32 Read32() = 0;

	virtual int GetPosition() = 0; // return values can be anything, as long as relative distances are correct
	virtual u8* GetRealCurrentPtr() = 0;
};

// =================================================================================================
// CDataReader_Fifo
// =================================================================================================

class CDataReader_Fifo : public IDataReader
{
private:

public:
    CDataReader_Fifo();
        
    virtual void Skip(u32);
    virtual u8 Read8();
    virtual u16 Read16();
    virtual u32 Read32();
	virtual int GetPosition();
	virtual u8* GetRealCurrentPtr();
};

// =================================================================================================
// CDataReader_Memory
// =================================================================================================

class CDataReader_Memory : public IDataReader
{
private:

    //u8* m_pMemory;
    u32 m_uReadAddress;

public:

    CDataReader_Memory(u32 _uAddress);

    u32 GetReadAddress();

    virtual void Skip(u32);
    virtual u8 Read8();
    virtual u16 Read16();
    virtual u32 Read32();
	virtual int GetPosition();
	virtual u8* GetRealCurrentPtr();
};

extern IDataReader* g_pDataReader;
#endif





#ifdef DATAREADER_INLINE
extern u8* g_pVideoData;
#endif

#ifdef DATAREADER_DEBUG
extern u8* g_pDataReaderRealPtr;
#define DATAREADER_DEBUG_CHECK_PTR 		g_pDataReaderRealPtr = g_pDataReader->GetRealCurrentPtr(); \
										if (g_pDataReaderRealPtr!=g_pVideoData) _asm int 3
#define DATAREADER_DEBUG_CHECK_PTR_VAL	g_pDataReaderRealPtr = g_pDataReader->GetRealCurrentPtr(); \
										if ((g_pDataReaderRealPtr != g_pVideoData) || (tmp != tmpdb)) _asm int 3
//#define DATAREADER_DEBUG_CHECK_PTR_VAL	DATAREADER_DEBUG_CHECK_PTR
#else
#define DATAREADER_DEBUG_CHECK_PTR 		
#define DATAREADER_DEBUG_CHECK_PTR_VAL	
#endif

#ifdef DATAREADER_INLINE
inline u8 DataPeek8(u32 _uOffset)
{
	u8 tmp = *(u8*)(g_pVideoData + _uOffset);
	return  tmp;
}
inline u16 DataPeek16(u32 _uOffset)
{
    u16 tmp = Common::swap16(*(u16*)(g_pVideoData + _uOffset));
	return  tmp;
}
inline u32 DataPeek32(u32 _uOffset)	{
    u32 tmp = Common::swap32(*(u32*)(g_pVideoData + _uOffset));
	return  tmp;
}

inline u8 DataReadU8()
{
	u8 tmp = *g_pVideoData;
	g_pVideoData++;
#ifdef DATAREADER_DEBUG
	u8 tmpdb = g_pDataReader->Read8();
	DATAREADER_DEBUG_CHECK_PTR_VAL;
#endif
	return tmp;
}

inline u16 DataReadU16()
{
    u16 tmp = Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData+=2;
#ifdef DATAREADER_DEBUG
	u16 tmpdb = g_pDataReader->Read16();
	DATAREADER_DEBUG_CHECK_PTR_VAL;
#endif
	return tmp;
}

inline u32 DataReadU32()
{
	u32 tmp = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData+=4;
#ifdef DATAREADER_DEBUG
	u32 tmpdb = g_pDataReader->Read32();
	DATAREADER_DEBUG_CHECK_PTR_VAL;
#endif
	return tmp;
}

inline float DataReadF32()
{
    union {u32 i; float f;} temp;
	temp.i = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData+=4;
	float tmp = temp.f;
#ifdef DATAREADER_DEBUG
	//TODO clean up
	u32 tmp3 = g_pDataReader->Read32();
	float tmpdb = *(float*)(&tmp3);
	DATAREADER_DEBUG_CHECK_PTR_VAL;
#endif
	return tmp;
}

inline u8* DataGetPosition()
{
#ifdef DATAREADER_DEBUG
	DATAREADER_DEBUG_CHECK_PTR;
#endif
	return g_pVideoData;
}

inline void DataSkip(u32 skip)
{
	g_pVideoData += skip;
#ifdef DATAREADER_DEBUG
	g_pDataReader->Skip(skip);
	DATAREADER_DEBUG_CHECK_PTR;
#endif
}
#else
inline u8 DataReadU8()
{
	u8 tmp = g_pDataReader->Read8();
	return tmp;
}
inline u16 DataReadU16()
{
	u16 tmp = g_pDataReader->Read16();
	return tmp;
}
inline u32 DataReadU32()
{
	u32 tmp = g_pDataReader->Read32();
	return tmp;
}
inline float DataReadF32()
{
	u32 tmp2 = g_pDataReader->Read32();
	float tmp = *(float*)(&tmp2);
	return tmp;
}
inline void DataSkip(u32 skip)
{
	g_pDataReader->Skip(skip);
}
#endif
#endif
