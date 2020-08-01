// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <deque>
#include <map>

#include "Common/Logging/Log.h"
#include "Core/API/Events.h"

#include "eventmodule.h"
#include "Scripting/Python/coroutine.h"
#include "Scripting/Python/Utils/convert.h"
#include "Scripting/Python/Utils/gil.h"
#include "Scripting/Python/Utils/invoke.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

// If you are looking for where the actual events are defined,
// scroll to the bottom of this file.

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
  static void Listener(const TEvent& event)
  {
    // We make the following assumption here:
    // - Events that originate from emulation (e.g. frameadvance)
    //   are emitted from the emulation thread.
    // - Events that do not originate from emulation are emitted from a different thread.
    // Under those circumstances we can always just directly invoke the listener
    // and have the desired effect of
    // a) emulation events being processed synchronously to emulation, and
    // b) concurrent events be processed concurrently.
    // The same assumptions apply to the one-time listeners registered in ScheduleCoroutine.

    if (callback.IsNull())
      return;
    const std::tuple<TsArgs...> args = TFunc(event);
    Py::GIL lock;
    PyObject* result =
        std::apply([&](auto&&... arg) { return Py::CallFunction(callback, arg...); }, args);
    if (result == nullptr)
    {
      PyErr_Print();
      return;
    }
    if (PyCoro_CheckExact(result))
      HandleNewCoroutine(Py::Wrap(result));
  }
  static PyObject* SetCallback(PyObject* newCallback)
  {
    if (newCallback == Py_None)
    {
      callback = Py::Null();
      Py_RETURN_NONE;
    }
    if (!PyCallable_Check(newCallback))
    {
      Py::GIL lock;
      PyErr_SetString(PyExc_TypeError, "event callback must be callable");
      return nullptr;
    }
    callback = Py::Take(newCallback);
    Py_RETURN_NONE;
  }
  static void ScheduleCoroutine(const Py::Object coro)
  {
    awaiting_coroutines.emplace_back(coro);
    API::GetEventHub().ListenEventOnce<TEvent>(NotifyAwaitingCoroutine);
  }
  static void NotifyAwaitingCoroutine(const TEvent& event)
  {
    if (awaiting_coroutines.empty())
      return; // queue might have been cleared
    // Since we schedule one notify event per coroutine,
    // we only ever need to process 1 item.
    const Py::Object coro = awaiting_coroutines.front();
    awaiting_coroutines.pop_front();
    const std::tuple<TsArgs...> args = TFunc(event);
    Py::GIL lock;
    PyObject* args_tuple = Py::BuildValueTuple(args);
    PyObject* newAsyncEventTuple = Py::CallMethod(coro, "send", args_tuple);
    if (newAsyncEventTuple != nullptr)
      HandleCoroutine(coro, Py::Wrap(newAsyncEventTuple));
    else if (!PyErr_ExceptionMatches(PyExc_StopIteration))
      // coroutines signal completion by raising StopIteration
      PyErr_Print();
  }
  static void Clear()
  {
    callback = Py::Null();
    awaiting_coroutines.clear();
  }
  static Py::Object callback;
  static std::deque<Py::Object> awaiting_coroutines;
};

template <typename TEvent, typename... TsArgs, MappingFunc<TEvent, TsArgs...> TFunc>
Py::Object PyEvent<MappingFunc<TEvent, TsArgs...>, TFunc>::callback = Py::Null();

template <typename TEvent, typename... TsArgs, MappingFunc<TEvent, TsArgs...> TFunc>
std::deque<Py::Object> PyEvent<MappingFunc<TEvent, TsArgs...>, TFunc>::awaiting_coroutines;

template <auto T>
struct PyEventFromMappingFunc : PyEvent<decltype(T), T>
{
};

template <class... Ts>
struct PythonEventContainer
{
public:
  static void RegisterListeners()
  {
    const auto listener_ids = std::apply(
        [&](auto&... event) {
          return std::make_tuple(API::GetEventHub().ListenEvent(event.Listener)...);
        },
        events);
    cleanup.emplace([listener_ids{std::move(listener_ids)}]() {
      std::apply(
          [&](const auto&... listener_id) { (API::GetEventHub().UnlistenEvent(listener_id), ...); },
          listener_ids);
    });
  }
  static void UnregisterListeners()
  {
    if (cleanup.has_value()) cleanup.value()();
    std::apply([&](const auto&... event) { (event.Clear(), ...); }, events);
  }
private:
  static const std::tuple<Ts...> events;
  static std::optional<std::function<void()>> cleanup;
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

// EVENT DEFINITIONS
// Creates a PyEvent class from the signature.
using PyFrameAdvanceEvent = PyEventFromMappingFunc<PyFrameAdvance>;

static PyMethodDef EventMethods[] = {
    // EVENT CALLBACKS
    // Has "on"-prefix, let's python code register a callback
    Py::MakeMethodDef<PyFrameAdvanceEvent::SetCallback>("onframeadvance"),

    {nullptr, nullptr, 0, nullptr} /* Sentinel */
};

// HOOKING UP PY EVENTS TO DOLPHIN EVENTS
// For all python events listed here, listens to the respective API::Events event
// deduced from the PyEvent signature's input argument.
using EventContainer = PythonEventContainer<PyFrameAdvanceEvent>;

std::optional<std::function<void(const Py::Object)>> GetCoroutineScheduler(std::string aeventname)
{
  static std::map<std::string, std::function<void(const Py::Object)>> lookup = {
      // HOOKING UP PY EVENTS TO AWAITABLE STRING REPRESENTATION
      // All async-awaitable events must be listed twice:
      // Here, and under the same name in the event_module_pycode
      {"frameadvance", PyFrameAdvanceEvent::ScheduleCoroutine}};
  auto iter = lookup.find(aeventname);
  if (iter == lookup.end())
    return std::optional<std::function<void(const Py::Object)>>{};
  else
    return iter->second;
}

inline const char* event_module_pycode = R"(

class _DolphinAsyncEvent:
    def __init__(self, event_name, *args):
        self.event_name = event_name
        self.args = args
    def __await__(self):
        return (yield ("dolphin_async_event_magic_string", self.event_name, self.args))

async def frameadvance():
    return (await _DolphinAsyncEvent("frameadvance"))

)";

/*********************************
 *  actual events defined above  *
 *********************************/

std::optional<std::function<void()>> EventContainer::cleanup;

PyMODINIT_FUNC PyInit_event()
{
  static PyModuleDef def = Py::MakeModuleDef("event", EventMethods);
  PyObject* m = PyModule_Create(&def);
  if (m == nullptr)
    return nullptr;
  return Py::LoadPyCodeIntoModule(m, event_module_pycode);
}

void InitPyListeners()
{
  EventContainer::RegisterListeners();
}

void ShutdownPyListeners()
{
  EventContainer::UnregisterListeners();
}

}  // namespace PyScripting
