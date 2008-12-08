// Copyright (C) 2003-2008 Dolphin Project.
// Copyright (C) Hector Martin "marcan" (hector@marcansoft.com)

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


#ifndef WIIMOTE_EXTENSION_ENCRYPTION_H
#define WIIMOTE_EXTENSION_ENCRYPTION_H


// ===================================================
/* They key structure to use with wiimote_gen_key() */
// ----------------
typedef struct {
	u8 ft[8];
	u8 sb[8];
} wiimote_key;


void wiimote_encrypt(wiimote_key *key, u8 *data, int addr, u8 len);

void wiimote_gen_key(wiimote_key *key, u8 *keydata);


#endif