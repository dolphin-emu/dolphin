// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <libusb.h>
#include <memory>
#include <mutex>

#include "Common/LibusbContext.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

namespace LibusbContext
{
static std::shared_ptr<libusb_context> s_libusb_context;
static std::once_flag s_context_initialized;

static libusb_context* Create()
{
  libusb_context* context;
  const int ret = libusb_init(&context);
  if (ret < LIBUSB_SUCCESS)
  {
    bool is_windows = false;
#ifdef _WIN32
    is_windows = true;
#endif
    const std::string reason =
        is_windows && ret == LIBUSB_ERROR_NOT_FOUND ?
            GetStringT("Failed to initialize libusb because usbdk is not installed.") :
            StringFromFormat(GetStringT("Failed to initialize libusb (%s).").c_str(),
                             libusb_error_name(ret));
    PanicAlertT("%s\nSome USB features will not work.", reason.c_str());
    return nullptr;
  }
  return context;
}

std::shared_ptr<libusb_context> Get()
{
  std::call_once(s_context_initialized, []() {
    s_libusb_context.reset(Create(), [](auto* context) {
      if (context != nullptr)
        libusb_exit(context);
    });
  });
  return s_libusb_context;
}
}
