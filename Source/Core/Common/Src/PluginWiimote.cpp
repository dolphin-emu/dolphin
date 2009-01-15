#include "PluginWiimote.h"

namespace Common {
    PluginWiimote::PluginWiimote(const char *_Filename) : CPlugin(_Filename), validWiimote(false) {
  
	Wiimote_ControlChannel = reinterpret_cast<TWiimote_Output>   
	    (LoadSymbol("Wiimote_ControlChannel"));
	Wiimote_InterruptChannel = reinterpret_cast<TWiimote_Input>
	    (LoadSymbol("Wiimote_InterruptChannel"));
	Wiimote_Update = reinterpret_cast<TWiimote_Update>
	    (LoadSymbol("Wiimote_Update"));
	Wiimote_GetAttachedControllers = reinterpret_cast<TWiimote_GetAttachedControllers>
	    (LoadSymbol("Wiimote_GetAttachedControllers"));

	if ((Wiimote_ControlChannel != 0) &&
	    (Wiimote_InterruptChannel != 0) &&
	    (Wiimote_Update != 0) &&
	    (Wiimote_GetAttachedControllers != 0))
	    validWiimote = true;
    }

    PluginWiimote::~PluginWiimote() {
    }
}
