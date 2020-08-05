// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "dolphinmodule.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

PyMODINIT_FUNC PyInit_dolphin()
{
  static const char dolphin_module_pycode[] = R"(
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

# defining __all__ let's people import everything
# using a star-import: from dolphin import *
__all__ = [event, memory, gui, savestate]
)";
  static PyMethodDef DolphinMethods[] = {
          // no functions defined in C++ code
          {nullptr, nullptr, 0, nullptr} /* Sentinel */
  };
  static PyModuleDef def = Py::MakeModuleDef("dolphin", DolphinMethods);
  PyObject* m = PyModule_Create(&def);
  if (m == nullptr)
    return nullptr;
  if (Py::LoadPyCodeIntoModule(m, dolphin_module_pycode).IsNull())
    return nullptr;
  return m;
}

}  // namespace PyScripting
