// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include <QWidget>

#include "UICommon/GameFile.h"

namespace DiscIO
{
class Volume;
}

class QComboBox;
class QGroupBox;
class QLineEdit;
class QPixmap;
class QTextEdit;

class InfoWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit InfoWidget(const UICommon::GameFile& game);
  ~InfoWidget() override;

private:
  void ChangeLanguage();
  void SaveBanner();

  QGroupBox* CreateFileDetails();
  QGroupBox* CreateGameDetails();
  QGroupBox* CreateBannerDetails();
  QLineEdit* CreateValueDisplay(const QString& value);
  QLineEdit* CreateValueDisplay(const std::string& value = "");
  void CreateLanguageSelector();
  QWidget* CreateBannerGraphic(const QPixmap& image);

  std::unique_ptr<DiscIO::Volume> m_volume;
  UICommon::GameFile m_game;
  QComboBox* m_language_selector;
  QLineEdit* m_name = {};
  QLineEdit* m_maker = {};
  QTextEdit* m_description = {};
};
