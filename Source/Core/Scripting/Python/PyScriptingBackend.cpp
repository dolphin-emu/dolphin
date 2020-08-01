// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Python.h>
#include <string>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/API/Events.h"
#include "DiscIO/Filesystem.h"
#include "PyScriptingBackend.h"

#include "Scripting/Python/coroutine.h"
#include "Scripting/Python/Modules/doliomodule.h"
#include "Scripting/Python/Modules/dolphinmodule.h"
#include "Scripting/Python/Modules/eventmodule.h"
#include "Scripting/Python/Modules/memorymodule.h"
#include "Scripting/Python/Utils/gil.h"

namespace PyScripting
{

const std::wstring python_home = UTF8ToWString(File::GetExeDirectory()) + L"/python-embed";
const std::wstring python_path =
    UTF8ToWString(File::GetExeDirectory()) + L"/python-embed/python38.zip;" +
    UTF8ToWString(File::GetExeDirectory()) + L"/python-embed;" +
    UTF8ToWString(File::GetExeDirectory());

void InitPythonInterpreter()
{

  if (PyImport_AppendInittab("dolio_stdout", PyInit_dolio_stdout) == -1)
    ERROR_LOG(SCRIPTING, "failed to add dolio_stdout to builtins");
  if (PyImport_AppendInittab("dolio_stderr", PyInit_dolio_stderr) == -1)
    ERROR_LOG(SCRIPTING, "failed to add dolio_stderr to builtins");
  if (PyImport_AppendInittab("dolphin_memory", PyInit_memory) == -1)
    ERROR_LOG(SCRIPTING, "failed to add dolphin_memory to builtins");
  if (PyImport_AppendInittab("dolphin_event", PyInit_event) == -1)
    ERROR_LOG(SCRIPTING, "failed to add dolphin_event to builtins");

  if (PyImport_AppendInittab("dolphin", PyInit_dolphin) == -1)
    ERROR_LOG(SCRIPTING, "failed to add dolphin to builtins");

  Py_SetPythonHome(const_cast<wchar_t*>(python_home.c_str()));
  Py_SetPath(python_path.c_str());
  INFO_LOG(SCRIPTING, "Initializing embedded python... %s", Py_GetVersion());
  Py_InitializeEx(0);
  PyObject* result_stdout = PyImport_ImportModule("dolio_stdout");
  if (result_stdout == nullptr)
    ERROR_LOG(SCRIPTING, "Error auto-importing dolio_stdout for stdout");
  PyObject* result_stderr = PyImport_ImportModule("dolio_stderr");
  if (result_stderr == nullptr)
    ERROR_LOG(SCRIPTING, "Error auto-importing dolio_stderr for stderr");

  if (PyGILState_Check())
  {
    // Apparently starting with Python 3.7, the main thread starts off with a locked GIL.
    // This might be the same issue: https://bugs.python.org/issue38680
    // We need to manually unlock the GIL here to start off in an unlocked state,
    // because we handle locking and unlocking the GIL with a RAII-wrapper.
    PyEval_SaveThread();
  }
}

void Init(std::filesystem::path script_filepath)
{
  InitPythonInterpreter();
  InitPyListeners();

  if (script_filepath.is_relative())
    script_filepath = File::GetExeDirectory() / script_filepath;
  std::string script_filepath_str = script_filepath.string();

  if (!std::filesystem::exists(script_filepath))
  {
    ERROR_LOG(SCRIPTING, "Script filepath was not found: %s", script_filepath_str.c_str());
    return;
  }

  Py::GIL lock;
  PyCompilerFlags flags = {PyCF_ALLOW_TOP_LEVEL_AWAIT};
  PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
  PyObject* execution_result =
      PyRun_FileExFlags(fopen(script_filepath_str.c_str(), "rb"), script_filepath_str.c_str(),
                        Py_file_input, globals, globals, true, &flags);

  if (execution_result == nullptr)
  {
    PyErr_Print();
    return;
  }

  if (PyCoro_CheckExact(execution_result))
    HandleNewCoroutine(Py::Wrap(execution_result));
}

void Shutdown()
{
  ShutdownPyListeners();

  PyGILState_Ensure();
  if (Py_FinalizeEx() != 0)
    PyErr_Print();
}

}  // namespace PyScripting
