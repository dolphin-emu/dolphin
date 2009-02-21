#ifndef _PLUGINWIIMOTE_H
#define _PLUGINWIIMOTE_H

#include "pluginspecs_wiimote.h"
#include "Plugin.h"

namespace Common {
    
typedef unsigned int (__cdecl* TPAD_GetAttachedPads)();
typedef void (__cdecl* TWiimote_Update)();
typedef void (__cdecl* TWiimote_Output)(u16 _channelID, const void* _pData, u32 _Size);
typedef void (__cdecl* TWiimote_Input)(u16 _channelID, const void* _pData, u32 _Size);
typedef unsigned int (__cdecl* TWiimote_GetAttachedControllers)();

class PluginWiimote : public CPlugin {
public:
	PluginWiimote(const char *_Filename);
	virtual ~PluginWiimote();
	virtual bool IsValid() {return validWiimote;};

	TWiimote_Output Wiimote_ControlChannel;
	TWiimote_Input  Wiimote_InterruptChannel;
	TWiimote_Update Wiimote_Update;
	TWiimote_GetAttachedControllers Wiimote_GetAttachedControllers;

private:
	bool validWiimote;
};

}  // namespace

#endif
