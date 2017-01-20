// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

// Used by early USB interfaces, such as /dev/usb/oh0 (except in IOS57, 58, 59) and /dev/usb/oh1.

namespace IOS
{
namespace HLE
{
struct IOCtlVRequest;

namespace USB
{
enum V0Requests
{
  IOCTLV_USBV0_CTRLMSG = 0,
  IOCTLV_USBV0_BLKMSG = 1,
  IOCTLV_USBV0_INTRMSG = 2,
};

struct V0CtrlMessage final : CtrlMessage
{
  explicit V0CtrlMessage(const IOCtlVRequest& ioctlv);
};

struct V0BulkMessage final : BulkMessage
{
  explicit V0BulkMessage(const IOCtlVRequest& ioctlv, bool long_length = false);
};

struct V0IntrMessage final : IntrMessage
{
  explicit V0IntrMessage(const IOCtlVRequest& ioctlv);
};
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
