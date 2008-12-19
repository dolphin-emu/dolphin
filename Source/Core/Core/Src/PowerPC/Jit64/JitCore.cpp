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
#include "JitCore.h"
#include "JitCache.h"
#include "JitAsm.h"
#include "Jit.h"

#include "../../HW/Memmap.h"
#include "../../HW/CPU.h"
#include "../../HW/DSP.h"
#include "../../HW/GPFifo.h"

#include "../../HW/VideoInterface.h"	
#include "../../HW/SerialInterface.h"	
#include "../../Core.h"

namespace JitCore
{

void Init()
{
	jit.Init();
	asm_routines.compareEnabled = ::Core::g_CoreStartupParameter.bRunCompareClient;
}

void Shutdown()
{
	jit.Shutdown();
}

void SingleStep()
{
	Run();
}

void Run()
{
	jit.EnterFastRun();		
}

}  // namespace
