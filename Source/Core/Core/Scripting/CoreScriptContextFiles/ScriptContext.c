#ifdef __cplusplus
extern "C" {
#endif

#include "Core/Scripting/CoreScriptContextFiles/ScriptContext.h"

const char* most_recent_script_version = "1.0.0";

ScriptContext* CreateScript(int unique_identifier, const char* script_file_name,
                            void (*print_callback_function)(const char*), void(*script))
{
  return NULL;
}

void ShutdownScript(ScriptContext* script_context)
{
  script_context->is_script_active = 0;
  script_context->scriptContextBaseFunctionsTable.script_end_callback(
      script_context, script_context->unique_script_identifier);
}

#ifdef __cplusplus
}
#endif
