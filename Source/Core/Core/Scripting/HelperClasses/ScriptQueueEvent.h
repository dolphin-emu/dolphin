#ifndef SCRIPT_QUEUE_EVENT
#define SCRIPT_QUEUE_EVENT

#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/HelperClasses/ScriptQueueEventTypes.h"

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

class ScriptQueueEvent
{
public:
  ScriptQueueEvent(const ScriptQueueEventTypes new_event_type,
                   const int new_unique_script_identifier, const char* new_script_filename,
                   const Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE&
                       new_print_callback_func,
                   const Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE&
                       new_script_end_callback_func)
  {
    event_type = new_event_type;
    unique_script_identifier = new_unique_script_identifier;
    script_filename = std::string(new_script_filename);
    print_callback_func = new_print_callback_func;
    script_end_callback_func = new_script_end_callback_func;
  }

  bool operator==(const ScriptQueueEvent& other_event) const
  {
    return this->event_type == other_event.event_type &&
           this->unique_script_identifier == other_event.unique_script_identifier &&
           this->script_filename == other_event.script_filename &&
           this->print_callback_func == other_event.print_callback_func &&
           this->script_end_callback_func == other_event.script_end_callback_func;
  }

  ScriptQueueEventTypes event_type;
  int unique_script_identifier;
  std::string script_filename;
  Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE print_callback_func;
  Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE script_end_callback_func;
};

#ifdef __cplusplus
}
#endif

#endif
