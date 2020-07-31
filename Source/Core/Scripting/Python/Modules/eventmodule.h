// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <Python.h>

#include "Scripting/Python/Utils/object_wrapper.h"

namespace PyScripting
{

PyMODINIT_FUNC PyInit_event();

std::optional<std::function<void(const Py::Object coro)>>
GetCoroutineScheduler(std::string aeventname);

void InitPyListeners();
void ShutdownPyListeners();

}
