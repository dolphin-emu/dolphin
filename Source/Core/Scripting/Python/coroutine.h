// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Scripting/Python/Utils/object_wrapper.h"

namespace PyScripting
{

// Handle a not-yet-started coroutine that was returned by normal
// script execution (top-level await) or an async callback.
// Those need to get started by initally calling "send" with None
// and then hand them over to HandleCoroutine.
void HandleNewCoroutine(const Py::Object module, const Py::Object obj);

// For an already-started coroutine and its event tuple describing what
// is being awaited, decode that tuple and make sure the coroutine gets
// resumed once the event being awaited is emitted.
void HandleCoroutine(const Py::Object module, const Py::Object coro, Py::Object asyncEventTuple);

}  // namespace PyScripting
