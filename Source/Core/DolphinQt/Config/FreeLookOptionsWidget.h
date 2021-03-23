// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include "Common/Config/ConfigInfo.h"

class QGroupBox;
class ToolTipDoubleSpinBox;

class FreeLookOptionsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FreeLookOptionsWidget(QWidget* parent, const Config::Info<float>& fov_horizontal,
                                 const Config::Info<float>& fov_vertical);

private:
  void CreateLayout();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  QGroupBox* CreateFieldOfViewGroup();

  const Config::Info<float>& m_fov_horizontal_config;
  const Config::Info<float>& m_fov_vertical_config;

  ToolTipDoubleSpinBox* m_fov_horizontal;
  ToolTipDoubleSpinBox* m_fov_vertical;
};
