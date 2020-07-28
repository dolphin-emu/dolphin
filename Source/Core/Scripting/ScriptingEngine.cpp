
#include "../Common/Logging/Log.h"
#include "../Core/API/Events.h"
#include "ScriptingEngine.h"


namespace Scripting
{

void Init(std::filesystem::path script_filepath)
{
  INFO_LOG(SCRIPTING, "Initializing scripting engine...");
  // TODO initialize scripting backend
}

void Shutdown()
{
  INFO_LOG(SCRIPTING, "Shutting down scripting engine...");
  // TODO shut down scripting backend
}

}  // namespace Scripting
