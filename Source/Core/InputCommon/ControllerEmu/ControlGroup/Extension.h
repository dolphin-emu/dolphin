// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
class EmulatedController;

class Extension : public ControlGroup
{
public:
  explicit Extension(const std::string& name);

  void GetState(u8* data);
  bool IsButtonPressed() const;

  std::vector<std::unique_ptr<EmulatedController>> attachments;

  int switch_extension = 0;
  int active_extension = 0;
};
}  // namespace ControllerEmu
