#include "EventHandler.h"

EventHandler *eventHandler = NULL;

namespace InputCommon
{
    void Init() {
	// init the event handler
	eventHandler = new EventHandler();
    }
    
    void Shutdown() {
	if (eventHandler)
	    delete eventHandler;
    }
}
