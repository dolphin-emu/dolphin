#include "PluginDSP.h"

namespace Common {

PluginDSP::PluginDSP(const char *_Filename) : CPlugin(_Filename), validDSP(false) {

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
