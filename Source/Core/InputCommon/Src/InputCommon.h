#include "EventHandler.h"

//extern EventHandler *eventHandler;

namespace InputCommon
{
enum EButtonType
{
	CTL_AXIS = 0,
	CTL_HAT,
	CTL_BUTTON,	
	CTL_KEY,
};

enum ETriggerType
{
	CTL_TRIGGER_SDL = 0,
	CTL_TRIGGER_XINPUT,
};

enum EXInputTrigger
{
	XI_TRIGGER_L = 0,
	XI_TRIGGER_R,
};

    void Init(); 
    void Shutdown();
}
