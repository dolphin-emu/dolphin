#pragma once

#include <filesystem>

namespace Scripting
{

class ScriptingBackend
{
public:
  ScriptingBackend(std::filesystem::path script_filepath);
  ~ScriptingBackend();

  ScriptingBackend(const ScriptingBackend&) = delete;
  ScriptingBackend& operator=(const ScriptingBackend&) = delete;
  ScriptingBackend(ScriptingBackend&&);
  ScriptingBackend& operator=(ScriptingBackend&&);
private:
  // We cannot name the actual used python scripting backend here,
  // as that would expose the Python.h header, which we don't want.
  // TODO help! how can I do this better??
  void* m_state;
};

} // namespace Scripting
