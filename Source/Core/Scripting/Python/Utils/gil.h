// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Convenience class for taking python's global interpreter lock using RAII.

#pragma once

#include <Python.h>

namespace Py
{

class GIL
{
  PyGILState_STATE gil_state;
public:
  GIL() { gil_state = PyGILState_Ensure(); }
  ~GIL() { PyGILState_Release(gil_state); }
};

}  // namespace Py
