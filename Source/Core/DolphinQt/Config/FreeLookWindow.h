// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class HardcoreWarningWidget;
class QDialogButtonBox;

class FreeLookWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit FreeLookWindow(QWidget* parent);

signals:
  void OpenAchievementSettings();

private:
  void CreateMainLayout();
  void ConnectWidgets();

  HardcoreWarningWidget* m_hc_warning;
  QDialogButtonBox* m_button_box;
};
