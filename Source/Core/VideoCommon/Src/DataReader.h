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

extern u8* g_pVideoData;

inline u8 DataPeek8(u32 _uOffset)
{
	return g_pVideoData[_uOffset];
}

inline u16 DataPeek16(u32 _uOffset)
{
    return Common::swap16(*(u16*)&g_pVideoData[_uOffset]);
}

inline u32 DataPeek32(u32 _uOffset)	
{
    return Common::swap32(*(u32*)&g_pVideoData[_uOffset]);
}

inline u8 DataReadU8()
{
	return *g_pVideoData++;
}

inline u16 DataReadU16()
{
    u16 tmp = Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

inline u32 DataReadU32()
{
	u32 tmp = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}

inline u32 DataReadU32Unswapped()
{
	u32 tmp = *(u32*)g_pVideoData;
	g_pVideoData += 4;
	return tmp;
}


// These are not used yet. If they don't build under Linux, delete them and let me know. -ector
template<class T>
inline T DataRead()
{
	T tmp = *(T*)g_pVideoData;
	g_pVideoData += sizeof(T);
	return tmp;
}

template <>
inline u16 DataRead()
{
    u16 tmp = Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

template <>
inline s16 DataRead()
{
    s16 tmp = (s16)Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

template <>
inline u32 DataRead()
{
    u32 tmp = (u32)Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}

template <>
inline s32 DataRead()
{
    s32 tmp = (s32)Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}


inline float DataReadF32()
{
    union {u32 i; float f;} temp;
	temp.i = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	float tmp = temp.f;
	return tmp;
}

inline u8* DataGetPosition()
{
	return g_pVideoData;
}

inline void DataSkip(u32 skip)
{
	g_pVideoData += skip;
}

#endif
