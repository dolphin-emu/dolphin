// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>
#include <wobjectdefs.h>

#include "DolphinQt2/GameList/GameFile.h"

class QComboBox;
class QGroupBox;
class QTextEdit;
class QLineEdit;

class InfoWidget final : public QWidget
{
  W_OBJECT(InfoWidget)
public:
  explicit InfoWidget(const GameFile& game);

private:
  void ComputeChecksum();
  W_SLOT(ComputeChecksum, W_Access::Private);
  void ChangeLanguage();
  W_SLOT(ChangeLanguage, W_Access::Private);
  void SaveBanner();
  W_SLOT(SaveBanner, W_Access::Private);

private:
  QGroupBox* CreateBannerDetails();
  QGroupBox* CreateISODetails();
  QLineEdit* CreateValueDisplay() { return CreateValueDisplay(QStringLiteral("")); };
  QLineEdit* CreateValueDisplay(const QString& value);
  QWidget* CreateChecksumComputer();
  void CreateLanguageSelector();
  QWidget* CreateBannerGraphic();

  GameFile m_game;
  QLineEdit* m_checksum_result;
  QComboBox* m_language_selector;
  QLineEdit* m_long_name;
  QLineEdit* m_short_name;
  QLineEdit* m_short_maker;
  QLineEdit* m_long_maker;
  QTextEdit* m_description;
};
