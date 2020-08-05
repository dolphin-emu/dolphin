// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Events.h"

namespace API
{
EventHub& GetEventHub()
{
  static EventHub event_hub;
  return event_hub;
}

}  // namespace API
