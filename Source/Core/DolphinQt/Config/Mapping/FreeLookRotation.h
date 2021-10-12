// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QGridLayout;

class FreeLookRotation final : public MappingWidget
{
  Q_OBJECT
public:
  explicit FreeLookRotation(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();

  // Main
  QGridLayout* m_main_layout;
};
