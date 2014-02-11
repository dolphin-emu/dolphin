// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
//
// Copyright (C) Hector Martin "marcan" (hector@marcansoft.com)

#pragma once

// ===================================================
/* They key structure to use with wiimote_gen_key() */
// ----------------
struct wiimote_key
{
	u8 ft[8];
	u8 sb[8];
};


void wiimote_encrypt(const wiimote_key* const key, u8* const data, int addr, const u8 len);
void wiimote_decrypt(const wiimote_key* const key, u8* const data, int addr, const u8 len);

void wiimote_gen_key(wiimote_key* const key, const u8* const keydata);
