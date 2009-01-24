#include "PluginPAD.h"

namespace Common
{
    PluginPAD::PluginPAD(const char *_Filename) : CPlugin(_Filename), validPAD(false)
	{  
		PAD_GetStatus = reinterpret_cast<TPAD_GetStatus>
			(LoadSymbol("PAD_GetStatus"));
		PAD_Input = reinterpret_cast<TPAD_Input>
			(LoadSymbol("PAD_Input"));
		PAD_Rumble = reinterpret_cast<TPAD_Rumble>
			(LoadSymbol("PAD_Rumble"));
		PAD_GetAttachedPads = reinterpret_cast<TPAD_GetAttachedPads>
			(LoadSymbol("PAD_GetAttachedPads"));

		if ((PAD_GetStatus != 0) &&
			(PAD_Input != 0) &&
			(PAD_Rumble != 0) &&
			(PAD_GetAttachedPads != 0))
			validPAD = true;
    }

    PluginPAD::~PluginPAD() {}
}
