// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <libusb.h>
#include <memory>

#include "Common/LibusbContext.h"
#include "Common/MsgHandler.h"

static libusb_context* CreateLibusbContext()
{
  libusb_context* context;
  const int ret = libusb_init(&context);
  if (ret < LIBUSB_SUCCESS)
  {
    PanicAlert("libusb_init failed: %s", libusb_error_name(ret));
    return nullptr;
  }
  return context;
}

namespace LibusbContext
{
static std::shared_ptr<libusb_context> s_libusb_context;
std::shared_ptr<libusb_context> Get()
{
  if (!s_libusb_context)
    s_libusb_context.reset(CreateLibusbContext(), [](auto* context) { libusb_exit(context); });
  return s_libusb_context;
}
}
