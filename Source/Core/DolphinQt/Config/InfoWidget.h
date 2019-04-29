// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <QWidget>

#include "UICommon/GameFile.h"

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

private:
  void ChangeLanguage();
  void SaveBanner();

  QGroupBox* CreateBannerDetails();
  QGroupBox* CreateISODetails();
  QLineEdit* CreateValueDisplay(const QString& value);
  QLineEdit* CreateValueDisplay(const std::string& value = "");
  void CreateLanguageSelector();
  QWidget* CreateBannerGraphic(const QPixmap& image);

  UICommon::GameFile m_game;
  QComboBox* m_language_selector;
  QLineEdit* m_name = {};
  QLineEdit* m_maker = {};
  QTextEdit* m_description = {};
};
