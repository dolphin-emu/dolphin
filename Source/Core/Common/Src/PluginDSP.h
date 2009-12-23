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

#ifndef _PLUGINDSP_H_
#define _PLUGINDSP_H_

#include "pluginspecs_dsp.h"
#include "Plugin.h"

namespace Common {

typedef void (__cdecl* TDSP_WriteMailBox)(bool _CPUMailbox, unsigned short);
typedef unsigned short (__cdecl* TDSP_ReadMailBox)(bool _CPUMailbox);
typedef unsigned short (__cdecl* TDSP_ReadControlRegister)();
typedef unsigned short (__cdecl* TDSP_WriteControlRegister)(unsigned short);
typedef void (__cdecl *TDSP_SendAIBuffer)(unsigned int address, unsigned int num_samples, unsigned int sample_rate);
typedef void (__cdecl *TDSP_Update)(int cycles);
typedef void (__cdecl *TDSP_StopSoundStream)();
typedef void (__cdecl *TDSP_ClearAudioBuffer)();

class PluginDSP : public CPlugin
{
public:
	PluginDSP(const char *_Filename);
	virtual ~PluginDSP();
	virtual bool IsValid() {return validDSP;};

	TDSP_ReadMailBox	         DSP_ReadMailboxHigh;
	TDSP_ReadMailBox	         DSP_ReadMailboxLow;
	TDSP_WriteMailBox	         DSP_WriteMailboxHigh;
	TDSP_WriteMailBox            DSP_WriteMailboxLow;
	TDSP_ReadControlRegister     DSP_ReadControlRegister;
	TDSP_WriteControlRegister    DSP_WriteControlRegister;
	TDSP_SendAIBuffer	         DSP_SendAIBuffer;
	TDSP_Update                  DSP_Update;
	TDSP_StopSoundStream         DSP_StopSoundStream;
	TDSP_ClearAudioBuffer        DSP_ClearAudioBuffer;

private:
	bool validDSP;

};

}  // namespace

#endif // _PLUGINDSP_H_
