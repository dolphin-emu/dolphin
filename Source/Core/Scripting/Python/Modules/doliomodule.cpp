// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>

#include "Common/Logging/Log.h"
#include "doliomodule.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

static std::stringstream buffer_stdout;
static std::stringstream buffer_stderr;

void flush_stdout()
{
  auto content = buffer_stdout.str();
  if (content.empty())
    return;
  NOTICE_LOG(SCRIPTING, "Script stdout: %s", content.c_str());
  buffer_stdout.str("");
}

void flush_stderr()
{
  auto content = buffer_stderr.str();
  if (content.empty())
    return;
  ERROR_LOG(SCRIPTING, "Script stderr: %s", content.c_str());
  buffer_stderr.str("");
}

void dol_write_stdout(const char* what)
{
  for (auto i = 0; what[i] != '\0'; ++i)
  {
    if (what[i] == '\n')
      flush_stdout();
    else
      buffer_stdout << what[i];
  }
}

void dol_write_stderr(const char* what)
{
  for (auto i = 0; what[i] != '\0'; ++i)
  {
    if (what[i] == '\n')
      flush_stderr();
    else
      buffer_stderr << what[i];
  }
}

static PyMethodDef IOMethodsStdout[] = {
    Py::MakeMethodDef<dol_write_stdout>("write"),
    Py::MakeMethodDef<flush_stdout>("flush"),
    {nullptr, nullptr, 0, nullptr}  // Sentinel
};

static PyMethodDef IOMethodsStderr[] = {
    Py::MakeMethodDef<dol_write_stderr>("write"),
    Py::MakeMethodDef<flush_stderr>("flush"),
    {nullptr, nullptr, 0, nullptr}  // Sentinel
};

static struct PyModuleDef iomodule_stdout = Py::MakeModuleDef("dolio_stdout", IOMethodsStdout);
static struct PyModuleDef iomodule_stderr = Py::MakeModuleDef("dolio_stderr", IOMethodsStderr);

PyMODINIT_FUNC PyInit_dolio_stdout()
{
  PyObject* m = PyModule_Create(&iomodule_stdout);
  if (m == nullptr)
    return nullptr;
  PySys_SetObject("stdout", m);
  return m;
}

PyMODINIT_FUNC PyInit_dolio_stderr()
{
  PyObject* m = PyModule_Create(&iomodule_stderr);
  if (m == nullptr)
    return nullptr;
  PySys_SetObject("stderr", m);
  return m;
}

}  // namespace PyScripting
