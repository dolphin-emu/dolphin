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
#ifndef _JITASM_H
#define _JITASM_H


namespace Jit64
{
	namespace Asm
	{
		extern const u8 *enterCode;

		extern const u8 *dispatcher;
		extern const u8 *dispatcherNoCheck;
		extern const u8 *dispatcherPcInEAX;

		extern const u8 *fpException;
		extern const u8 *computeRc;
		extern const u8 *computeRcFp;
		extern const u8 *testExceptions;
		extern const u8 *dispatchPcInEAX;
		extern const u8 *doTiming;

		extern const u8 *fifoDirectWrite8;
		extern const u8 *fifoDirectWrite16;
		extern const u8 *fifoDirectWrite32;
		extern const u8 *fifoDirectWriteFloat;
		extern const u8 *fifoDirectWriteXmm64;

		extern bool compareEnabled;
		void Generate();
	}
}

#endif

