#include "EventHandler.h"

EventHandler *eventHandler = NULL;

namespace InputCommon
{
	void Init()
	{
#if defined GLTEST && GLTEST
		// init the event handler
		eventHandler = new EventHandler();
#endif
	}

	void Shutdown()
	{
		if (eventHandler)
			delete eventHandler;
	}
}
