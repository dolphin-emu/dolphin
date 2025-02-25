// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <variant>

#include "Common/BitUtils.h"

#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"
#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"
#include "Core/HW/WiimoteEmu/Extension/Turntable.h"
#include "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"

namespace WiimoteEmu
{
struct DesiredExtensionState
{
private:
  template <typename... Ts>
  struct ExtDataImpl
  {
    using type = std::variant<std::monostate, typename Ts::DesiredState...>;
  };

public:
  using ExtensionData = ExtDataImpl<Nunchuk, Classic, Guitar, Drums, Turntable, UDrawTablet,
                                    DrawsomeTablet, TaTaCon, Shinkansen>::type;

  ExtensionData data = std::monostate{};
};

template <typename T>
void DefaultExtensionUpdate(EncryptedExtension::Register* reg,
                            const DesiredExtensionState& target_state)
{
  if (std::holds_alternative<T>(target_state.data))
  {
    Common::BitCastPtr<T>(&reg->controller_data) = std::get<T>(target_state.data);
  }
  else
  {
    Common::BitCastPtr<T>(&reg->controller_data) = T{};
  }
}
}  // namespace WiimoteEmu
