// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "coroutine.h"

#include <map>

#include "Common/Logging/Log.h"
#include "Core/API/Events.h"

#include "Scripting/Python/Modules/eventmodule.h"
#include "Scripting/Python/PyScriptingBackend.h"
#include "Scripting/Python/Utils/invoke.h"

namespace PyScripting
{

void HandleNewCoroutine(const Py::Object module, const Py::Object coro) {
  PyObject *asyncEventTuple = Py::CallMethod(coro, "send", Py::Take(Py_None).Leak());
  if (asyncEventTuple != nullptr)
  {
    HandleCoroutine(module, coro, Py::Wrap(asyncEventTuple));
  }
  else if (!PyErr_ExceptionMatches(PyExc_StopIteration))
  {
    // coroutines signal completion by raising StopIteration
    PyErr_Print();
  }
}

void HandleCoroutine(const Py::Object module, const Py::Object coro, const Py::Object asyncEventTuple)
{
  const char* magic_string;
  const char* event_name;
  PyObject* args_tuple;
  if (!PyArg_ParseTuple(asyncEventTuple.Lend(), "ssO", &magic_string, &event_name, &args_tuple))
  {
    ERROR_LOG_FMT(SCRIPTING, "A coroutine was yielded to the emulator that it cannot process. "
                         "Did you await something that isn't a dolphin event? "
                         "(error: await-tuple was not (str, str, args))");
    return;
  }
  if (std::string(magic_string) != "dolphin_async_event_magic_string")
  {
    ERROR_LOG_FMT(SCRIPTING, "A coroutine was yielded to the emulator that it cannot process. "
                         "Did you await something that isn't a dolphin event? "
                         "(error: wrong magic string to identify as dolphin-native event)");
    return;
  }
  // `args_tuple` is unused:
  // right now there are no events that take in arguments.
  // If there were, say `await frameadvance(5)` to wait 5 frames,
  // those arguments would be passed as a tuple via `args_tuple`.

  auto scheduler_opt = GetCoroutineScheduler(event_name);
  if (!scheduler_opt.has_value())
  {
    ERROR_LOG_FMT(SCRIPTING, "An unknown event was tried to be awaited: {}", event_name);
    return;
  }
  std::function<void(const Py::Object, const Py::Object)> scheduler = scheduler_opt.value();
  scheduler(module, coro);
}

}  // namespace PyScripting
