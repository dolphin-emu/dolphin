#include "ScriptContext.h"

namespace Scripting
{
bool set_print_callback = false;
bool set_script_end_callback = false;
std::function<void(const std::string&)>* print_callback = nullptr;
std::function<void(int)>* script_end_callback = nullptr;
ThreadSafeQueue<ScriptContext*> queue_of_scripts_waiting_to_start = ThreadSafeQueue<ScriptContext*>();


bool IsPrintCallbackSet()
{
  return set_print_callback;
}

bool IsScriptEndCallbackSet()
{
  return set_script_end_callback;
}

// We only set the print callback and script callback for the ScriptWindow UI on the first time a
// script is loaded, since they're the same 2 functions for all scripts.
void SetPrintCallback(std::function<void(const std::string&)>* new_print_callback)
{
  if (!set_print_callback)
  {
    print_callback = new_print_callback;
    set_print_callback = true;
  }
}

void SetScriptEndCallback(std::function<void(int)>* new_script_end_callback)
{
  if (!set_script_end_callback)
  {
    script_end_callback = new_script_end_callback;
    set_script_end_callback = true;
  }
}

std::function<void(const std::string&)>* GetPrintCallback()
{
  return print_callback;
}

std::function<void(int)>* GetScriptEndCallback()
{
  return script_end_callback;
}

void AddScriptToQueueOfScriptsWaitingToStart(ScriptContext* new_script)
{
  queue_of_scripts_waiting_to_start.push(new_script);
}

ScriptContext* RemoveNextScriptToStartFromQueue()
{
  return queue_of_scripts_waiting_to_start.pop();
}
}
