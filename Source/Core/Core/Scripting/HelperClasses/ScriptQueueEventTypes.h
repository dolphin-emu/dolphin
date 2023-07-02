#ifndef SCRIPT_QUEUE_EVENT_TYPES
#define SCRIPT_QUEUE_EVENT_TYPES

#ifdef __cplusplus
extern "C" {
#endif

enum ScriptQueueEventTypes
{
  CreateScriptFromUI = 0,
  StopScriptFromUI = 1,
  StopScriptFromScriptEndCallback = 2,
  Unknown = 3
};

#ifdef __cplusplus
}
#endif

#endif
