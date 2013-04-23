// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_IOBRIDGE_H_
#define _WII_IOBRIDGE_H_

#include "Common.h"
class PointerWrap;

namespace WII_IOBridge
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);

void Update();

void Read8(u8& _rReturnValue, const u32 _Address);
void Read16(u16& _rReturnValue, const u32 _Address);
void Read32(u32& _rReturnValue, const u32 _Address);
void Read64(u64& _rReturnValue, const u32 _Address);

void Write8(const u8 _Value, const u32 _Address);
void Write16(const u16 _Value, const u32 _Address);
void Write32(const u32 _Value, const u32 _Address);
void Write64(const u64 _Value, const u32 _Address);

} // end of namespace AudioInterface

#endif


