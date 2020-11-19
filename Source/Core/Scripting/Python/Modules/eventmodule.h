// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <optional>
#include <Python.h>

#include "Scripting/Python/Utils/object_wrapper.h"

namespace PyScripting
{

PyMODINIT_FUNC PyInit_event();

using CoroutineScheduler = void(*)(const Py::Object, const Py::Object);
std::optional<CoroutineScheduler> GetCoroutineScheduler(std::string aeventname);

}
