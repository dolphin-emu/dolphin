// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "Core/HW/WiimoteEmu/ExtensionPort.h"

class QGroupBox;
class QHBoxLayout;

class WiimoteEmuExtension final : public MappingWidget
{
  Q_OBJECT
public:
  explicit WiimoteEmuExtension(MappingWindow* window);

  InputConfig* GetConfig() override;

  void ChangeExtensionType(u32 type);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateClassicLayout();
  void CreateDrumsLayout();
  void CreateGuitarLayout();
  void CreateNoneLayout();
  void CreateNunchukLayout();
  void CreateTurntableLayout();
  void CreateUDrawTabletLayout();
  void CreateDrawsomeTabletLayout();
  void CreateTaTaConLayout();
  void CreateShinkansenLayout();
  void CreateMainLayout();

  // Main
  QHBoxLayout* m_main_layout;
  QGroupBox* m_classic_box;
  QGroupBox* m_drums_box;
  QGroupBox* m_guitar_box;
  QGroupBox* m_none_box;
  QGroupBox* m_nunchuk_box;
  QGroupBox* m_turntable_box;
  QGroupBox* m_udraw_tablet_box;
  QGroupBox* m_drawsome_tablet_box;
  QGroupBox* m_tatacon_box;
  QGroupBox* m_shinkansen_box;
};
