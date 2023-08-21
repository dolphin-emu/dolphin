// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QCheckBox;
class QFormLayout;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

class GCMicrophone final : public MappingWidget
{
  Q_OBJECT
public:
  explicit GCMicrophone(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();

  QHBoxLayout* m_main_layout;
};
