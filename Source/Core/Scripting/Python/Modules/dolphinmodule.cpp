// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "dolphinmodule.h"

#include "Common/Logging/Log.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

struct DolphinModuleState
{
};

static void SetupDolphinModule(PyObject* module, DolphinModuleState* state)
{
  static const char pycode[] = R"(
"""
This module is an aggregator of all dolphin-provided modules.
It lets people import the dolphin-provided modules in a more
intuitive way. For example, people can then do this:
    from dolphin import event, memory
instead of:
    import dolphin_event as event
    import dolphin_memory as memory
"""

import dolphin_event as event
import dolphin_memory as memory
import dolphin_gui as gui
import dolphin_savestate as savestate
import dolphin_controller as controller
import dolphin_registers as registers

# defining __all__ let's people import everything
# using a star-import: from dolphin import *
__all__ = [event, memory, gui, savestate, controller, registers]
)";
  Py::Object result = Py::LoadPyCodeIntoModule(module, pycode);
  if (result.IsNull())
  {
    ERROR_LOG_FMT(SCRIPTING, "Failed to load embedded python code into dolphin module");
  }
}

PyMODINIT_FUNC PyInit_dolphin()
{
  static PyMethodDef methods[] = {
      // no functions defined in C++ code
      {nullptr, nullptr, 0, nullptr}  // Sentinel
  };
  static PyModuleDef module_def =
      Py::MakeStatefulModuleDef<DolphinModuleState, SetupDolphinModule>("dolphin", methods);
  PyObject* def_obj = PyModuleDef_Init(&module_def);
  return def_obj;
}

}  // namespace PyScripting
