#ifndef _PLUGINDSP_H
#define _PLUGINDSP_H

#include "pluginspecs_dsp.h"
#include "Plugin.h"

namespace Common {

typedef void (__cdecl* TDSP_WriteMailBox)(bool _CPUMailbox, unsigned short);
typedef unsigned short (__cdecl* TDSP_ReadMailBox)(bool _CPUMailbox);
typedef unsigned short (__cdecl* TDSP_ReadControlRegister)();
typedef unsigned short (__cdecl* TDSP_WriteControlRegister)(unsigned short);
typedef void (__cdecl* TDSP_SendAIBuffer)(unsigned int address, int sample_rate);
typedef void (__cdecl* TDSP_Update)(int cycles);
typedef void (__cdecl* TDSP_StopSoundStream)();

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

private:
	bool validDSP;

};

}  // namespace

#endif
