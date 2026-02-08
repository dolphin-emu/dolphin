// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QLineEdit;
class QTableWidget;

class TriforceBaseboardSettingsDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit TriforceBaseboardSettingsDialog(QWidget* parent);

private:
  void LoadConfig();
  void SaveConfig();

  void LoadDefault();
  void OnClear();

  QTableWidget* m_ip_overrides_table;
};
