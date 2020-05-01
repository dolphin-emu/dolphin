// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"

class QCheckBox;
class QComboBox;
class QDateTimeEdit;

class MiscPane final : public QWidget
{
  Q_OBJECT
public:
  explicit MiscPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void Update();

  QComboBox* m_cpu_emulation_engine_combobox;
  ConfigBool* m_enable_mmu_checkbox;
  ConfigBool* m_pause_on_panic_checkbox;
  ConfigBool* m_accurate_cpu_cache_checkbox;

  QCheckBox* m_custom_rtc_checkbox;
  QDateTimeEdit* m_custom_rtc_datetime;
};
