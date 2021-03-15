// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

class InputConfig;

namespace ControllerEmu
{
class ControlGroup;
class Buttons;
}  // namespace ControllerEmu

enum class FreeLookGroup
{
  Move,
  Speed,
  FieldOfView,
  Other
};

namespace FreeLook
{
void Shutdown();
void Initialize();
void LoadInputConfig();
bool IsInitialized();
void UpdateInput();

InputConfig* GetInputConfig();
ControllerEmu::ControlGroup* GetInputGroup(int pad_num, FreeLookGroup group);

}  // namespace FreeLook

class FreeLookController final : public ControllerEmu::EmulatedController
{
public:
  FreeLookController(unsigned int index, const Config::Info<float>& fov_horizontal,
                     const Config::Info<float>& fov_vertical);

  std::string GetName() const override;
  void LoadDefaults(const ControllerInterface& ciface) override;

  ControllerEmu::ControlGroup* GetGroup(FreeLookGroup group) const;
  void Update();

private:
  const Config::Info<float>& m_fov_horizontal;
  const Config::Info<float>& m_fov_vertical;

  ControllerEmu::Buttons* m_move_buttons;
  ControllerEmu::Buttons* m_speed_buttons;
  ControllerEmu::Buttons* m_fov_buttons;
  ControllerEmu::Buttons* m_other_buttons;

  const unsigned int m_index;
};
