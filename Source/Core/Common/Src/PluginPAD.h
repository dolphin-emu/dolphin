#ifndef _PLUGINPAD_H
#define _PLUGINPAD_H

#include "pluginspecs_pad.h"
#include "Plugin.h"

namespace Common {
    typedef void (__cdecl* TPAD_GetStatus)(u8, SPADStatus*);
    typedef void (__cdecl* TPAD_Input)(u16, u8);
    typedef void (__cdecl* TPAD_Rumble)(u8, unsigned int, unsigned int);
    typedef unsigned int (__cdecl* TPAD_GetAttachedPads)();
    
    class PluginPAD : public CPlugin {
    public:
	PluginPAD(const char *_Filename);
	~PluginPAD();
	virtual bool IsValid() {return validPAD;};

	TPAD_GetStatus PAD_GetStatus;
	TPAD_Input PAD_Input;
	TPAD_Rumble PAD_Rumble;
	TPAD_GetAttachedPads PAD_GetAttachedPads;

    private:
	bool validPAD;

    };
}

#endif
