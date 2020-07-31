
#include "Common/Logging/Log.h"
#include "Python/PyScriptingBackend.h"
#include "ScriptingEngine.h"

namespace Scripting
{

void Init(std::filesystem::path script_filepath)
{
  INFO_LOG(SCRIPTING, "Initializing scripting engine...");
  PyScripting::Init(script_filepath);
}

void Shutdown()
{
  INFO_LOG(SCRIPTING, "Shutting down scripting engine...");
  PyScripting::Shutdown();
}

}  // namespace Scripting
