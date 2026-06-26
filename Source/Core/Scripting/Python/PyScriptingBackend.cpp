// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "PyScriptingBackend.h"

#include <Python.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Scripting/Python/coroutine.h"
#include "Scripting/Python/Modules/controllermodule.h"
#include "Scripting/Python/Modules/debugmodule.h"
#include "Scripting/Python/Modules/doliomodule.h"
#include "Scripting/Python/Modules/dolphinmodule.h"
#include "Scripting/Python/Modules/dtmmodule.h"
#include "Scripting/Python/Modules/eventmodule.h"
#include "Scripting/Python/Modules/utilmodule.h"
#include "Scripting/Python/Modules/guimodule.h"
#include "Scripting/Python/Modules/memorymodule.h"
#include "Scripting/Python/Modules/registersmodule.h"
#include "Scripting/Python/Modules/savestatemodule.h"
#include "Scripting/ScriptingEngine.h"

namespace PyScripting
{

namespace
{
#ifdef _WIN32
void AddExistingPythonPath(std::vector<std::wstring>* paths, const std::string& path)
{
  if (path.empty())
    return;

  if (!File::Exists(path) && !File::IsDirectory(path))
    return;

  paths->push_back(UTF8ToWString(path));
}

std::string GetBundledPythonHome()
{
  return File::GetExeDirectory() + "/python-embed";
}

std::wstring GetPythonHome()
{
  const std::string bundled_home = GetBundledPythonHome();
  if (File::IsDirectory(bundled_home))
    return UTF8ToWString(bundled_home);

  return UTF8ToWString(DOLPHIN_PYTHON_HOME);
}

std::vector<std::wstring> GetPythonPaths()
{
  std::vector<std::wstring> python_paths;

  AddExistingPythonPath(&python_paths, File::GetCurrentDir());

  const std::string bundled_home = GetBundledPythonHome();
  if (File::IsDirectory(bundled_home))
  {
    AddExistingPythonPath(&python_paths, bundled_home + "/" DOLPHIN_PYTHON_ZIP);
    AddExistingPythonPath(&python_paths, bundled_home + "/python.zip");
    AddExistingPythonPath(&python_paths, bundled_home + "/DLLs");
    AddExistingPythonPath(&python_paths, bundled_home + "/Lib");
    AddExistingPythonPath(&python_paths, bundled_home + "/Lib/site-packages");
    AddExistingPythonPath(&python_paths, bundled_home);
  }
  else
  {
    AddExistingPythonPath(&python_paths, DOLPHIN_PYTHON_DLLS);
    AddExistingPythonPath(&python_paths, DOLPHIN_PYTHON_STDLIB);
    AddExistingPythonPath(&python_paths, DOLPHIN_PYTHON_SITE_PACKAGES);
    AddExistingPythonPath(&python_paths, DOLPHIN_PYTHON_HOME);
  }

  AddExistingPythonPath(&python_paths, File::GetExeDirectory());
  AddExistingPythonPath(&python_paths, File::GetUserPath(D_SCRIPTS_IDX));

  return python_paths;
}
#endif

FILE* OpenScriptFile(const std::filesystem::path& path)
{
#ifdef _WIN32
  return _wfopen(path.wstring().c_str(), L"rb");
#else
  return fopen(path.c_str(), "rb");
#endif
}

void LogPythonStatusError(const char* context, const PyStatus& status)
{
  if (!PyStatus_Exception(status))
    return;

  ERROR_LOG_FMT(SCRIPTING, "{}: {}", context, status.err_msg ? status.err_msg : "unknown error");
}
}  // namespace

static PyThreadState* InitMainPythonInterpreter()
{
#ifdef _WIN32
  const std::wstring python_home = GetPythonHome();
  const std::vector<std::wstring> python_paths = GetPythonPaths();
#endif

  if (PyImport_AppendInittab("dolio_stdout", PyInit_dolio_stdout) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolio_stdout to builtins");
  if (PyImport_AppendInittab("dolio_stderr", PyInit_dolio_stderr) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolio_stderr to builtins");
  if (PyImport_AppendInittab("dolphin_memory", PyInit_memory) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_memory to builtins");
  if (PyImport_AppendInittab("dolphin_debug", PyInit_debug) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_debug to builtins");
  if (PyImport_AppendInittab("dolphin_event", PyInit_event) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_event to builtins");
  if (PyImport_AppendInittab("dolphin_gui", PyInit_gui) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_gui to builtins");
  if (PyImport_AppendInittab("dolphin_savestate", PyInit_savestate) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_savestate to builtins");
  if (PyImport_AppendInittab("dolphin_controller", PyInit_controller) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_controller to builtins");
  if (PyImport_AppendInittab("dolphin_dtm", PyInit_dtm) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_dtm to builtins");
  if (PyImport_AppendInittab("dolphin_registers", PyInit_registers) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_registers to builtins");
  if (PyImport_AppendInittab("dolphin_utils", PyInit_dol_utils) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin_utils to builtins");

  if (PyImport_AppendInittab("dolphin", PyInit_dolphin) == -1)
    ERROR_LOG_FMT(SCRIPTING, "failed to add dolphin to builtins");

  INFO_LOG_FMT(SCRIPTING, "Initializing embedded python... {}", Py_GetVersion());
  PyConfig config;
  PyConfig_InitPythonConfig(&config);
#ifdef _WIN32
  const PyStatus home_status = PyConfig_SetString(&config, &config.home, python_home.c_str());
  if (PyStatus_Exception(home_status))
  {
    LogPythonStatusError("Failed to set Python home", home_status);
    PyConfig_Clear(&config);
    return nullptr;
  }

  config.module_search_paths_set = 1;
  for (const auto& path : python_paths)
  {
    const PyStatus append_status = PyWideStringList_Append(&config.module_search_paths, path.c_str());
    if (PyStatus_Exception(append_status))
    {
      LogPythonStatusError("Failed to add Python module path", append_status);
      PyConfig_Clear(&config);
      return nullptr;
    }
  }
#endif

  const PyStatus init_status = Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);
  if (PyStatus_Exception(init_status))
  {
    LogPythonStatusError("Failed to initialize embedded Python", init_status);
    return nullptr;
  }

  // Starting with Python 3.7 Py_Initialize* also initializes the GIL in a locked state.
  // This might be the same issue: https://bugs.python.org/issue38680
  // We need to manually unlock the GIL here to start off in an unlocked state,
  // because we handle locking and unlocking the GIL with a RAII-wrapper.
  return PyEval_SaveThread();
}

static void Init(std::filesystem::path script_filepath)
{
  if (script_filepath.is_relative())
    script_filepath = std::filesystem::path(File::GetCurrentDir()) / script_filepath;
  std::string script_filepath_str = script_filepath.string();

  if (!std::filesystem::exists(script_filepath))
  {
    ERROR_LOG_FMT(SCRIPTING, "Script filepath was not found: {}", script_filepath_str.c_str());
    return;
  }

  FILE* script_file = OpenScriptFile(script_filepath);
  if (script_file == nullptr)
  {
    ERROR_LOG_FMT(SCRIPTING, "Could not open script file {}: {}", script_filepath_str.c_str(),
                  std::strerror(errno));
    return;
  }

  Py::Object globals = Py::Wrap(PyDict_New());
  if (globals.IsNull())
  {
    fclose(script_file);
    PyErr_Print();
    return;
  }

  const auto set_global = [&](const char* name, PyObject* value) {
    if (value == nullptr)
      return false;
    return PyDict_SetItemString(globals.Lend(), name, value) == 0;
  };

  Py::Object py_name = Py::Wrap(PyUnicode_FromString("__main__"));
  Py::Object py_file = Py::Wrap(PyUnicode_FromString(script_filepath_str.c_str()));
  if (!set_global("__builtins__", PyEval_GetBuiltins()) || !set_global("__name__", py_name.Lend()) ||
      !set_global("__file__", py_file.Lend()))
  {
    fclose(script_file);
    PyErr_Print();
    return;
  }

  PyCompilerFlags flags = {PyCF_ALLOW_TOP_LEVEL_AWAIT};
  Py::Object execution_result = Py::Wrap(PyRun_FileExFlags(
      script_file, script_filepath_str.c_str(), Py_file_input, globals.Lend(), globals.Lend(), true,
      &flags));

  if (execution_result.IsNull())
  {
    PyErr_Print();
    return;
  }

  if (PyCoro_CheckExact(execution_result.Lend()))
  {
    Py::Object event_module = Py::Wrap(PyImport_ImportModule("dolphin_event"));
    HandleNewCoroutine(event_module, execution_result);
  }
}

static void ShutdownMainPythonInterpreter()
{
  if (Py_FinalizeEx() != 0)
  {
    ERROR_LOG_FMT(SCRIPTING, "Unexpectedly failed to finalize python");
  }
}

PyScriptingBackend::PyScriptingBackend(std::filesystem::path script_filepath,
                                       API::EventHub& event_hub, API::Gui& gui,
                                       API::GCManip& gc_manip,
                                       API::WiiButtonsManip& wii_buttons_manip,
                                       API::WiiIRManip& wii_ir_manip,
                                       API::WiiAccelManip& wii_accel_manip,
                                       API::WiiMotionPlusManip& wii_motion_plus_manip,
                                       API::NunchuckButtonsManip& nunchuck_buttons_manip,
                                       API::NunchuckAccelManip& nunchuck_accel_manip)
    : m_event_hub(event_hub), m_gui(gui), m_gc_manip(gc_manip),
      m_wii_buttons_manip(wii_buttons_manip), m_wii_ir_manip(wii_ir_manip),
      m_wii_accel_manip(wii_accel_manip), m_wii_motion_plus_manip(wii_motion_plus_manip),
      m_nunchuck_buttons_manip(nunchuck_buttons_manip),
      m_nunchuck_accel_manip(nunchuck_accel_manip)
{
  std::lock_guard lock{s_bookkeeping_lock};
  if (s_instances.empty())
  {
    s_main_threadstate = InitMainPythonInterpreter();
    if (s_main_threadstate == nullptr)
      return;
  }
  PyEval_RestoreThread(s_main_threadstate);

  const bool no_subinterpreters = Scripting::ScriptingBackend::PythonSubinterpretersDisabled();
  if (no_subinterpreters)
  {
    m_interp_threadstate = s_main_threadstate;
  }
  else
  {
    m_interp_threadstate = Py_NewInterpreter();
    if (m_interp_threadstate == nullptr)
    {
      ERROR_LOG_FMT(SCRIPTING, "Failed to create Python subinterpreter");
      PyEval_SaveThread();
      return;
    }
    PyThreadState_Swap(m_interp_threadstate);
  }
  u64 interp_id = PyInterpreterState_GetID(m_interp_threadstate->interp);
  s_instances[interp_id] = this;

  {
    // new scope because we need to drop these PyObjects before we release the GIL
    // below (PyEval_SaveThread) because DECREF-ing them needs the GIL to be held.
    Py::Object result_stdout = Py::Wrap(PyImport_ImportModule("dolio_stdout"));
    if (result_stdout.IsNull())
    {
      ERROR_LOG_FMT(SCRIPTING, "Error auto-importing dolio_stdout for stdout");
      PyErr_Print();
    }
    Py::Object result_stderr = Py::Wrap(PyImport_ImportModule("dolio_stderr"));
    if (result_stderr.IsNull())
    {
      ERROR_LOG_FMT(SCRIPTING, "Error auto-importing dolio_stderr for stderr");
      PyErr_Print();
    }
  }

  Init(script_filepath);

  PyEval_SaveThread();
}

PyScriptingBackend::~PyScriptingBackend()
{
  std::lock_guard lock{s_bookkeeping_lock};
  if (m_interp_threadstate == nullptr)
    return;  // we've been moved from (if moving was implemented)
  m_gui.RemoveWidgetsForOwner(this);
  PyEval_RestoreThread(m_interp_threadstate);
  u64 interp_id = PyInterpreterState_GetID(m_interp_threadstate->interp);

  if (Scripting::ScriptingBackend::PythonSubinterpretersDisabled())
  {
    // cleanup in subinterpreter-less mode is a bit more complicated:
    // We cannot simply shut down the interpreter, so the modules will stay alive.
    // But we _do_ want to "stop" the modules, or else removing or reloading the script won't work.
    // We let modules define custom reset behaviour in a magic method "_dolphin_reset".
    // Right now all we need to do is reset the event module, which just unregisters all events.
    const char* modules_with_resets[] = {"dolphin_event", "dolphin_controller"};
    for (const auto& module_name : modules_with_resets)
    {
      Py::Object module = Py::Wrap(PyImport_ImportModule(module_name));
      if (module.IsNull())
      {
        ERROR_LOG_FMT(SCRIPTING, "Error importing {}", module_name);
        PyErr_Print();
        continue;
      }
      Py::Object reset_func = Py::Wrap(PyObject_GetAttrString(module.Lend(), "_dolphin_reset"));
      if (reset_func.IsNull())
      {
        WARN_LOG_FMT(SCRIPTING, "Expected a method called _dolphin_reset in {}, but was not found",
                     module_name);
        PyErr_Clear();
        continue;
      }
      Py::Object args = Py::Wrap(PyTuple_New(0));
      if (args.IsNull())
      {
        PyErr_Print();
        continue;
      }
      Py::Object call_result = Py::Wrap(PyObject_Call(reset_func.Lend(), args.Lend(), nullptr));
      if (call_result.IsNull())
      {
        ERROR_LOG_FMT(SCRIPTING, "Error calling _dolphin_reset for {}", module_name);
        PyErr_Print();
      }
    }
  }

  for (const auto& cleanup_func : m_cleanups)
    cleanup_func();
  m_cleanups.clear();

  API::ClearAllControllerInputManipulations();

  // Cleanup did remove listeners, but there may still be a concurrent iteration over the listeners happening.
  // We need to wait for all concurrent events to finish first (without holding the GIL to avoid deadlocks),
  // or else we might crash. See also https://github.com/Felk/dolphin/issues/12
  PyEval_SaveThread();
  GetEventHub()->TickAllListeners();
  PyEval_RestoreThread(m_interp_threadstate);

  // We are typically running without subinterpreters if we're using a python library that doesn't
  // support it. Some of those libraries, e.g. numpy, also break if their initialization routines
  // run more than once. So it's best to go full conservative and keep the interpreter alive for the
  // application's entire lifetime. See also https://stackoverflow.com/a/7676916
  if (!Scripting::ScriptingBackend::PythonSubinterpretersDisabled())
  {
    s_instances.erase(interp_id);
    Py_EndInterpreter(m_interp_threadstate);
  }

  PyThreadState_Swap(s_main_threadstate);
  if (s_instances.empty())
  {
    ShutdownMainPythonInterpreter();
    s_main_threadstate = nullptr;
  }
  else
  {
    PyEval_SaveThread();
  }
}

// Each PyScriptingBackend manages one python sub-interpreter.
// But python's C api is stateful instead of object oriented,
// so we need this lookup to bridge the gap.
PyScriptingBackend* PyScriptingBackend::GetCurrent()
{
  PyInterpreterState* interp_state = PyThreadState_Get()->interp;
  u64 interp_id = PyInterpreterState_GetID(interp_state);
  return s_instances[interp_id];
}

API::EventHub* PyScriptingBackend::GetEventHub()
{
  return &m_event_hub;
}

API::Gui* PyScriptingBackend::GetGui()
{
  return &m_gui;
}

API::GCManip* PyScriptingBackend::GetGCManip()
{
  return &m_gc_manip;
}

API::WiiButtonsManip* PyScriptingBackend::GetWiiButtonsManip()
{
  return &m_wii_buttons_manip;
}

API::WiiIRManip* PyScriptingBackend::GetWiiIRManip()
{
  return &m_wii_ir_manip;
}

API::WiiAccelManip* PyScriptingBackend::GetWiiAccelManip()
{
  return &m_wii_accel_manip;
}

API::WiiMotionPlusManip* PyScriptingBackend::GetWiiMotionPlusManip()
{
  return &m_wii_motion_plus_manip;
}

API::NunchuckButtonsManip* PyScriptingBackend::GetNunchuckButtonsManip()
{
  return &m_nunchuck_buttons_manip;
}

API::NunchuckAccelManip* PyScriptingBackend::GetNunchuckAccelManip()
{
  return &m_nunchuck_accel_manip;
}

void PyScriptingBackend::AddCleanupFunc(std::function<void()> cleanup_func)
{
  m_cleanups.push_back(cleanup_func);
}

std::map<u64, PyScriptingBackend*> PyScriptingBackend::s_instances;
PyThreadState* PyScriptingBackend::s_main_threadstate;
std::mutex PyScriptingBackend::s_bookkeeping_lock;

}  // namespace PyScripting
