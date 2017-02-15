// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

struct libusb_context;

namespace LibusbContext
{
// libusb on Windows is limited to only a single context. Trying to open more
// than one can cause issues with device enumerations.
// libusb is thread-safe so this context can be safely used from different threads.
std::shared_ptr<libusb_context> Get();
}
