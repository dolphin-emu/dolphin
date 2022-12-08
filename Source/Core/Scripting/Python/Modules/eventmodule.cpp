// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "eventmodule.h"

#include <deque>
#include <functional>
#include <map>

#include "Common/Logging/Log.h"
#include "Core/API/Events.h"

#include "Scripting/Python/coroutine.h"
#include "Scripting/Python/Utils/convert.h"
#include "Scripting/Python/Utils/invoke.h"
#include "Scripting/Python/Utils/module.h"
#include "Scripting/Python/PyScriptingBackend.h"

namespace PyScripting
{

// If you are looking for where the actual events are defined,
// scroll to the bottom of this file.

struct EventModuleState
{
  API::EventHub* event_hub;
  std::optional<std::function<void()>> cleanup_listeners;
  Py::Object callback;
  std::deque<Py::Object> awaiting_coroutines;
};

// These template shenanigans are all required for PyEventFromMappingFunc
// to be able to infer all of the mapping function signature's parts
// from the mapping function only.
// Basically it takes an <auto T>,
// turns that into <decltype(T), T> captured as <MappingFunc<TEvent, TsArgs...>, TFunc>,
// which then turns that into <TEvent, TsArgs..., MappingFunc<TEvent, TsArgs...> TFunc>.
// See https://stackoverflow.com/a/50775214/3688648 for a more elaborate explanation.
template <typename TEvent, typename... TsArgs>
using MappingFunc = const std::tuple<TsArgs...> (*)(const TEvent&);

template <typename T, T>
struct PyEvent;

template <typename TEvent, typename... TsArgs, MappingFunc<TEvent, TsArgs...> TFunc>
struct PyEvent<MappingFunc<TEvent, TsArgs...>, TFunc>
{
  static std::function<void(const TEvent&)> GetListener(const Py::Object module)
  {
    PyThreadState* threadstate = PyThreadState_Get();
    return [=](const TEvent& event) {
      PyEval_RestoreThread(threadstate);
      Listener(module, event);
      PyEval_SaveThread();
    };
  }
  static void Listener(const Py::Object module, const TEvent& event)
  {
    // We make the following assumption here:
    // - Events that originate from emulation (e.g. frameadvance)
    //   are emitted from the emulation thread.
    // - Events that do not originate from emulation are emitted from a different thread.
    // Under those circumstances we can always just directly invoke the listener
    // and have the desired effect of
    // a) emulation events being processed synchronously to emulation, and
    // b) concurrent events be processed concurrently.
    EventModuleState* state = Py::GetState<EventModuleState>(module.Lend());
    NotifyAwaitingCoroutines(module, event);
    if (state->callback.IsNull())
      return;
    const std::tuple<TsArgs...> args = TFunc(event);
    PyObject* result =
        std::apply([&](auto&&... arg) { return Py::CallFunction(state->callback, arg...); }, args);
    if (result == nullptr)
    {
      PyErr_Print();
      return;
    }
    if (PyCoro_CheckExact(result))
      HandleNewCoroutine(module, Py::Wrap(result));
  }
  static PyObject* SetCallback(PyObject* module, PyObject* newCallback)
  {
    EventModuleState* state = Py::GetState<EventModuleState>(module);
    if (newCallback == Py_None)
    {
      state->callback = Py::Null();
      Py_RETURN_NONE;
    }
    if (!PyCallable_Check(newCallback))
    {
      PyErr_SetString(PyExc_TypeError, "event callback must be callable");
      return nullptr;
    }
    state->callback = Py::Take(newCallback);
    Py_RETURN_NONE;
  }
  static void ScheduleCoroutine(Py::Object module, const Py::Object coro)
  {
    EventModuleState* state = Py::GetState<EventModuleState>(module.Lend());
    state->awaiting_coroutines.emplace_back(coro);
  }
  static void NotifyAwaitingCoroutines(const Py::Object module, const TEvent& event)
  {
    EventModuleState* state = Py::GetState<EventModuleState>(module.Lend());
    std::deque<Py::Object> awaiting_coroutines;
    std::swap(state->awaiting_coroutines, awaiting_coroutines);
    while (!awaiting_coroutines.empty())
    {
      const Py::Object coro = awaiting_coroutines.front();
      awaiting_coroutines.pop_front();
      const std::tuple<TsArgs...> args = TFunc(event);
      PyObject* args_tuple = Py::BuildValueTuple(args);
      PyObject* newAsyncEventTuple = Py::CallMethod(coro, "send", args_tuple);
      if (newAsyncEventTuple != nullptr)
        HandleCoroutine(module, coro, Py::Wrap(newAsyncEventTuple));
      else if (!PyErr_ExceptionMatches(PyExc_StopIteration))
        // coroutines signal completion by raising StopIteration
        PyErr_Print();
    }
  }
  static void Clear(EventModuleState* state)
  {
    state->callback = Py::Null();
    state->awaiting_coroutines.clear();
  }
};

template <auto T>
struct PyEventFromMappingFunc : PyEvent<decltype(T), T>
{
};

template <class... Ts>
struct PythonEventContainer
{
public:
  static void RegisterListeners(const Py::Object module)
  {
    EventModuleState* state = Py::GetState<EventModuleState>(module.Lend());
    const auto listener_ids = std::apply(
        [&](auto&&... pyevent) {
          return std::make_tuple(state->event_hub->ListenEvent(pyevent.GetListener(module))...);
        },
        s_pyevents);
    state->cleanup_listeners.emplace([=]() {
      std::apply([&](const auto&... listener_id) { (state->event_hub->UnlistenEvent(listener_id), ...); },
          listener_ids);
    });
  }
  static void UnregisterListeners(EventModuleState* state)
  {
    state->cleanup_listeners.value()();
    state->cleanup_listeners.reset();
    std::apply([&](const auto&... pyevent) { (pyevent.Clear(state), ...); }, s_pyevents);
  }
private:
  static const std::tuple<Ts...> s_pyevents;
};

/*********************************
 *  actual events defined below  *
 *********************************/

// EVENT MAPPING FUNCTIONS
// Turns an API::Events event to a std::tuple.
// The tuple represents the python event signature.
static const std::tuple<> PyFrameAdvance(const API::Events::FrameAdvance& evt)
{
  return std::make_tuple();
}
static const std::tuple<bool, u32, u64> PyMemoryBreakpoint(const API::Events::MemoryBreakpoint& evt)
{
  return std::make_tuple(evt.write, evt.addr, evt.value);
}
static const std::tuple<u32> PyCodeBreakpoint(const API::Events::CodeBreakpoint& evt)
{
  return std::make_tuple(evt.addr);
}
// EVENT DEFINITIONS
// Creates a PyEvent class from the signature.
using PyFrameAdvanceEvent = PyEventFromMappingFunc<PyFrameAdvance>;
using PyMemoryBreakpointEvent = PyEventFromMappingFunc<PyMemoryBreakpoint>;
using PyCodeBreakpointEvent = PyEventFromMappingFunc<PyCodeBreakpoint>;

// HOOKING UP PY EVENTS TO DOLPHIN EVENTS
// For all python events listed here, listens to the respective API::Events event
// deduced from the PyEvent signature's input argument.
using EventContainer =
    PythonEventContainer<PyFrameAdvanceEvent, PyMemoryBreakpointEvent, PyCodeBreakpointEvent>;
template <>
const std::tuple<PyFrameAdvanceEvent, PyMemoryBreakpointEvent, PyCodeBreakpointEvent>
    EventContainer::s_pyevents = {};

std::optional<CoroutineScheduler> GetCoroutineScheduler(std::string aeventname)
{
  static std::map<std::string, CoroutineScheduler> lookup = {
      // HOOKING UP PY EVENTS TO AWAITABLE STRING REPRESENTATION
      // All async-awaitable events must be listed twice:
      // Here, and under the same name in the setup python code
      {"frameadvance", PyFrameAdvanceEvent::ScheduleCoroutine},
      {"memorybreakpoint", PyMemoryBreakpointEvent::ScheduleCoroutine},
      {"codebreakpoint", PyCodeBreakpointEvent::ScheduleCoroutine},
  };
  auto iter = lookup.find(aeventname);
  if (iter == lookup.end())
    return std::optional<CoroutineScheduler>{};
  else
    return iter->second;
}

static void SetupEventModule(PyObject* module, EventModuleState* state)
{
  static const char pycode[] = R"(
class _DolphinAsyncEvent:
    def __init__(self, event_name, *args):
        self.event_name = event_name
        self.args = args
    def __await__(self):
        return (yield ("dolphin_async_event_magic_string", self.event_name, self.args))

async def frameadvance():
    return (await _DolphinAsyncEvent("frameadvance"))

async def memorybreakpoint():
    return (await _DolphinAsyncEvent("memorybreakpoint"))

async def codebreakpoint():
    return (await _DolphinAsyncEvent("codebreakpoint"))
)";
  Py::Object result = Py::LoadPyCodeIntoModule(module, pycode);
  if (result.IsNull())
  {
    ERROR_LOG_FMT(SCRIPTING, "Failed to load embedded python code into event module");
  }
  API::EventHub* event_hub = PyScripting::PyScriptingBackend::GetCurrent()->GetEventHub();
  state->event_hub = event_hub;
  const std::function cleanup = [state] { EventContainer::UnregisterListeners(state); };
  PyScripting::PyScriptingBackend::GetCurrent()->AddCleanupFunc(cleanup);
  EventContainer::RegisterListeners(Py::Take(module));
}

static PyObject* Reset(PyObject* module)
{
  EventModuleState* state = Py::GetState<EventModuleState>(module);
  state->callback = Py::Null();
  state->awaiting_coroutines.clear();
  Py_RETURN_NONE;
}

PyMODINIT_FUNC PyInit_event()
{
  static PyMethodDef methods[] = {
      // EVENT CALLBACKS
      // Has "on_"-prefix, let's python code register a callback
      Py::MakeMethodDef<PyFrameAdvanceEvent::SetCallback>("on_frameadvance"),
      Py::MakeMethodDef<PyMemoryBreakpointEvent::SetCallback>("on_memorybreakpoint"),
      Py::MakeMethodDef<PyCodeBreakpointEvent::SetCallback>("on_codebreakpoint"),
      Py::MakeMethodDef<Reset>("_dolphin_reset"),

      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<EventModuleState, SetupEventModule>("event", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
