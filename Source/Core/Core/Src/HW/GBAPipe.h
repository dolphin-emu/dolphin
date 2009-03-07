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
//  Sets up and maintains interprocess communication to make it possible
//  to easily compare any two cpu cores by running two instances of DolphinHLE and 
//  comparing them to each other.
//

#ifndef _GBAPIPE_H
#define _GBAPIPE_H

#include "Common.h"

namespace GBAPipe
{
	// start the server
	void StartServer();

	// connect as client
	void ConnectAsClient();

	// stop
	void Stop();

	// Transfer funcs
	void Read(u8& data);
	void Write(u8 data);

	// IsEnabled
	bool IsEnabled();

	// IsServer
	bool IsServer();

	void SetBlockStart(u32 addr);
}

#endif
