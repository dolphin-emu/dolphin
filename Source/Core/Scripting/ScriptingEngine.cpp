
#include "Core/API/Events.h"
#include "Core/API/Gui.h"
#include "Common/Logging/Log.h"
#include "Python/PyScriptingBackend.h"
#include "ScriptingEngine.h"

namespace Scripting
{

ScriptingBackend::ScriptingBackend(std::filesystem::path script_filepath)
{
  INFO_LOG(SCRIPTING, "Initializing scripting engine...");
  // There is only support for python right now.
  // If there was support for multiple backend, a fitting one could be
  // detected based on the script file's extension for example.
  m_state = new PyScripting::PyScriptingBackend(script_filepath, API::GetEventHub(), API::GetGui());
}

ScriptingBackend::~ScriptingBackend() {
  if (m_state != nullptr)
  {
    INFO_LOG(SCRIPTING, "Shutting down scripting engine...");
    delete m_state;
    m_state = nullptr;
  }
}

ScriptingBackend::ScriptingBackend(ScriptingBackend&& other)
{
  m_state = other.m_state;
  other.m_state = nullptr;
}

ScriptingBackend& ScriptingBackend::operator=(ScriptingBackend&& other)
{
  m_state = other.m_state;
  other.m_state = nullptr;
  return *this;
}

}  // namespace Scripting
