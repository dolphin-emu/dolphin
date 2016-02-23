#pragma once

#include "Common/Logging/LogManager.h"
#include "DolphinWatch.h"

namespace DolphinWatch {

	class RemoteListener : public LogListener
	{
	public:
		~RemoteListener() {}
		void RemoteListener::Log(LogTypes::LOG_LEVELS level, const char* msg)
		{
			DolphinWatch::Log(level, msg);
		}
	};

}
