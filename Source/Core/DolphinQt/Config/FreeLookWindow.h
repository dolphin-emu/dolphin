// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#ifdef USE_RETRO_ACHIEVEMENTS
class HardcoreWarningWidget;
#endif  // USE_RETRO_ACHIEVEMENTS
class QDialogButtonBox;

class FreeLookWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit FreeLookWindow(QWidget* parent);

#ifdef USE_RETRO_ACHIEVEMENTS
signals:
  void OpenAchievementSettings();
#endif  // USE_RETRO_ACHIEVEMENTS

private:
  void CreateMainLayout();
  void ConnectWidgets();

#ifdef USE_RETRO_ACHIEVEMENTS
  HardcoreWarningWidget* m_hc_warning;
#endif  // USE_RETRO_ACHIEVEMENTS
  QDialogButtonBox* m_button_box;
};
