// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "DolphinQt/Config/SettingsWindow.h"

namespace UICommon
{
class GameFile;
}

class PropertiesDialog final : public TabbedSettingsWindow
{
  Q_OBJECT
public:
  explicit PropertiesDialog(QWidget* parent, const UICommon::GameFile& game);
  const std::string& GetFilePath() const { return m_filepath; }

signals:
  void OpenGeneralSettings();
  void OpenGraphicsSettings();
#ifdef USE_RETRO_ACHIEVEMENTS
  void OpenAchievementSettings();
#endif  // USE_RETRO_ACHIEVEMENTS

private:
  const std::string m_filepath;
};
