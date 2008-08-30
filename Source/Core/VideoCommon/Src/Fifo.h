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

#ifndef _FIFO_H
#define _FIFO_H

#include "pluginspecs_video.h"

#include "Common.h"
#include "ChunkFile.h"

// inline for speed!
class FifoReader
{
    u8 *ptr;
    u8 *end;
    u8 *tempPtr; //single element stack :P
    u8 *tempEnd;
    bool pushed;
public:
    void Init(u8 *_ptr, u8 *_end)
    {
        ptr = _ptr; end = _end; pushed = false;
    }
    bool IsPushed() {return pushed;}
    void Push(u8 *_ptr, u8 *_end) {pushed = true; tempPtr = ptr; tempEnd = end; ptr = _ptr; end = _end;}
    void Pop() {pushed = false; ptr = tempPtr; end = tempEnd;}
    u8  Peek8 (int offset) const { return ptr[offset]; }
    u16 Peek16(int offset) const { return Common::swap16(*(u16*)(ptr+offset)); }
    u32 Peek32(int offset) const { return Common::swap32(*(u32*)(ptr+offset)); }
    u8  Read8 () {return *ptr++;}
    u16 Read16() {const u16 value = Common::swap16(*((u16*)ptr)); ptr+=2; return value;}
    u32 Read32() {const u32 value = Common::swap32(*((u32*)ptr)); ptr+=4; return value;}
    float Read32F() {const u32 value = Common::swap32(*((u32*)ptr)); ptr+=4; return *(float*)&value;}
    int GetRemainSize() const { return (int)(end - ptr); }
    u8 *GetPtr() const { return ptr; }
    void MoveEndForward() { end += 32; }
    u8 *GetEnd() const { return end; }
};

extern FifoReader fifo;

void Fifo_Init();
void Fifo_Shutdown();
void Fifo_EnterLoop(const SVideoInitialize &video_initialize);
void Fifo_DoState(PointerWrap &f);

#endif

