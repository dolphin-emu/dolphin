// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "dolphinmodule.h"
#include "Scripting/Python/Utils/module.h"

namespace PyScripting
{

inline const char* dolphin_module_pycode = R"(
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

# defining __all__ let's people import everything
# using a star-import: from dolphin import *
__all__ = [event, memory]

)";

static PyMethodDef DolphinMethods[] = {
    // no functions defined in C++ code
    {nullptr, nullptr, 0, nullptr} /* Sentinel */
};

PyMODINIT_FUNC PyInit_dolphin()
{
  static PyModuleDef def = Py::MakeModuleDef("dolphin", DolphinMethods);
  PyObject* m = PyModule_Create(&def);
  if (m == nullptr)
    return nullptr;
  return Py::LoadPyCodeIntoModule(m, dolphin_module_pycode);
}

}  // namespace PyScripting
