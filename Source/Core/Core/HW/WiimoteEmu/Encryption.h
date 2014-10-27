// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
//
// Copyright (C) Hector Martin "marcan" (hector@marcansoft.com)

#pragma once

#include "Common/CommonTypes.h"

// The key structure to use with WiimoteGenerateKey()
struct wiimote_key
{
	u8 ft[8];
	u8 sb[8];
};


void WiimoteEncrypt(const wiimote_key* const key, u8* const data, int addr, const u8 len);
void WiimoteDecrypt(const wiimote_key* const key, u8* const data, int addr, const u8 len);

void WiimoteGenerateKey(wiimote_key* const key, const u8* const keydata);
