// Copyright (C) 2003-2009 Dolphin Project.

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

// TODO: Handle cache-is-full condition :p


#include "Common.h"
#include "DLCache.h"

namespace DLCache
{

void Init()
{
}

void Shutdown()
{
}

void Clear() 
{
}

void ProgressiveCleanup()
{
}
}  // namespace

// NOTE - outside the namespace on purpose.
bool HandleDisplayList(u32 address, u32 size)
{
	return false;
}

void IncrementCheckContextId()
{
}
