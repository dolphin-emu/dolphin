// Copyright (C) 2003 Dolphin Project.

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

#include "PluginDSP.h"

namespace Common {

PluginDSP::PluginDSP(const char *_Filename)
: CPlugin(_Filename), validDSP(false)
{
	DSP_ReadMailboxHigh      = reinterpret_cast<TDSP_ReadMailBox>
		(LoadSymbol("DSP_ReadMailboxHigh"));
	DSP_ReadMailboxLow       = reinterpret_cast<TDSP_ReadMailBox>
		(LoadSymbol("DSP_ReadMailboxLow"));
	DSP_WriteMailboxHigh     = reinterpret_cast<TDSP_WriteMailBox>
		(LoadSymbol("DSP_WriteMailboxHigh"));
	DSP_WriteMailboxLow      = reinterpret_cast<TDSP_WriteMailBox>
		(LoadSymbol("DSP_WriteMailboxLow"));
	DSP_ReadControlRegister  = reinterpret_cast<TDSP_ReadControlRegister>
		(LoadSymbol("DSP_ReadControlRegister"));
	DSP_WriteControlRegister = reinterpret_cast<TDSP_WriteControlRegister>
		(LoadSymbol("DSP_WriteControlRegister"));
	DSP_Update               = reinterpret_cast<TDSP_Update>
		(LoadSymbol("DSP_Update"));
	DSP_SendAIBuffer         = reinterpret_cast<TDSP_SendAIBuffer>
		(LoadSymbol("DSP_SendAIBuffer"));
	DSP_StopSoundStream      = reinterpret_cast<TDSP_StopSoundStream>
		(LoadSymbol("DSP_StopSoundStream"));

	if ((DSP_ReadMailboxHigh != 0) &&
		(DSP_ReadMailboxLow != 0) &&
		(DSP_WriteMailboxHigh != 0) &&
		(DSP_WriteMailboxLow != 0) &&
		(DSP_ReadControlRegister != 0) &&
		(DSP_WriteControlRegister != 0) &&
		(DSP_SendAIBuffer != 0) &&
		(DSP_Update != 0) &&
		(DSP_StopSoundStream != 0))
		validDSP = true;
}

PluginDSP::~PluginDSP() {
}

}  // namespace
