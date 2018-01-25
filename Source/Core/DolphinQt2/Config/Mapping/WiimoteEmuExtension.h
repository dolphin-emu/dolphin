// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Mapping/MappingWidget.h"

class QGroupBox;
class QHBoxLayout;

class WiimoteEmuExtension final : public MappingWidget
{
public:
  enum class Type
  {
    NONE,
    CLASSIC_CONTROLLER,
    DRUMS,
    GUITAR,
    NUNCHUK,
    TURNTABLE
  };

  explicit WiimoteEmuExtension(MappingWindow* window);

  InputConfig* GetConfig() override;

  void ChangeExtensionType(Type type);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateClassicLayout();
  void CreateDrumsLayout();
  void CreateGuitarLayout();
  void CreateNoneLayout();
  void CreateNunchukLayout();
  void CreateTurntableLayout();
  void CreateMainLayout();

  // Main
  QHBoxLayout* m_main_layout;
  QGroupBox* m_classic_box;
  QGroupBox* m_drums_box;
  QGroupBox* m_guitar_box;
  QGroupBox* m_none_box;
  QGroupBox* m_nunchuk_box;
  QGroupBox* m_turntable_box;
};
