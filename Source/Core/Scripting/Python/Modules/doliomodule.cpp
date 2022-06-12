// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "doliomodule.h"

#include <sstream>

#include "Common/Logging/Log.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

struct DolioModuleState
{
  std::stringstream buffer;
};

static void flush_stdout(PyObject* module)
{
  DolioModuleState* state = Py::GetState<DolioModuleState>(module);
  auto content = state->buffer.str();
  if (content.empty())
    return;
  NOTICE_LOG_FMT(SCRIPTING, "Script stdout: {}", content.c_str());
  state->buffer.str("");
}

static void flush_stderr(PyObject* module)
{
  DolioModuleState* state = Py::GetState<DolioModuleState>(module);
  auto content = state->buffer.str();
  if (content.empty())
    return;
  ERROR_LOG_FMT(SCRIPTING, "Script stderr: {}", content.c_str());
  state->buffer.str("");
}

static void dol_write_stdout(PyObject* module, const char* what)
{
  DolioModuleState* state = Py::GetState<DolioModuleState>(module);
  for (auto i = 0; what[i] != '\0'; ++i)
  {
    if (what[i] == '\n')
      flush_stdout(module);
    else
      state->buffer << what[i];
  }
}

static void dol_write_stderr(PyObject* module, const char* what)
{
  DolioModuleState* state = Py::GetState<DolioModuleState>(module);
  for (auto i = 0; what[i] != '\0'; ++i)
  {
    if (what[i] == '\n')
      flush_stderr(module);
    else
      state->buffer << what[i];
  }
}

static void setup_dolio_module_stdout(PyObject* module, DolioModuleState* state)
{
  PySys_SetObject("stdout", module);
}

static void setup_dolio_module_stderr(PyObject* module, DolioModuleState* state)
{
  PySys_SetObject("stderr", module);
}

PyMODINIT_FUNC PyInit_dolio_stdout()
{
  static PyMethodDef method_defs[] = {
      Py::MakeMethodDef<dol_write_stdout>("write"),
      Py::MakeMethodDef<flush_stdout>("flush"),
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<DolioModuleState, setup_dolio_module_stdout>("dolio_stdout", method_defs);
  return PyModuleDef_Init(&module_def);
}

PyMODINIT_FUNC PyInit_dolio_stderr()
{
  static PyMethodDef methods[] = {
      Py::MakeMethodDef<dol_write_stderr>("write"),
      Py::MakeMethodDef<flush_stderr>("flush"),
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<DolioModuleState, setup_dolio_module_stderr>("dolio_stderr", methods);
  return PyModuleDef_Init(&module_def);
}

}  // namespace PyScripting
