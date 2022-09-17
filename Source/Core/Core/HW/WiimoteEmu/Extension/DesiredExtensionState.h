// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>
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
#include "Core/HW/WiimoteEmu/ExtensionPort.h"

namespace WiimoteEmu
{
struct DesiredExtensionState
{
  using ExtensionData =
      std::variant<std::monostate, Nunchuk::DataFormat, Classic::DataFormat, Guitar::DataFormat,
                   Drums::DesiredState, Turntable::DataFormat, UDrawTablet::DataFormat,
                   DrawsomeTablet::DataFormat, TaTaCon::DataFormat, Shinkansen::DesiredState>;
  ExtensionData data = std::monostate();

  static_assert(std::is_same_v<std::monostate,
                               std::variant_alternative_t<ExtensionNumber::NONE, ExtensionData>>);
  static_assert(
      std::is_same_v<Nunchuk::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::NUNCHUK, ExtensionData>>);
  static_assert(
      std::is_same_v<Classic::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::CLASSIC, ExtensionData>>);
  static_assert(std::is_same_v<Guitar::DataFormat,
                               std::variant_alternative_t<ExtensionNumber::GUITAR, ExtensionData>>);
  static_assert(std::is_same_v<Drums::DesiredState,
                               std::variant_alternative_t<ExtensionNumber::DRUMS, ExtensionData>>);
  static_assert(
      std::is_same_v<Turntable::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::TURNTABLE, ExtensionData>>);
  static_assert(
      std::is_same_v<UDrawTablet::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::UDRAW_TABLET, ExtensionData>>);
  static_assert(
      std::is_same_v<DrawsomeTablet::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::DRAWSOME_TABLET, ExtensionData>>);
  static_assert(
      std::is_same_v<TaTaCon::DataFormat,
                     std::variant_alternative_t<ExtensionNumber::TATACON, ExtensionData>>);
  static_assert(
      std::is_same_v<Shinkansen::DesiredState,
                     std::variant_alternative_t<ExtensionNumber::SHINKANSEN, ExtensionData>>);
  static_assert(std::variant_size_v<DesiredExtensionState::ExtensionData> == ExtensionNumber::MAX);
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
