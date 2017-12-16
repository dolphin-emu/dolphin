// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/GeckoCode.h"

class CheatWarningWidget;
class GameFile;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QPushButton;

class GeckoCodeWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GeckoCodeWidget(const GameFile& game);

signals:
  void OpenGeneralSettings();

private:
  void OnSelectionChanged();
  void OnItemChanged(QListWidgetItem* item);

  void CreateWidgets();
  void ConnectWidgets();
  void UpdateList();
  void DownloadCodes();
  void SaveCodes();

  const GameFile& m_game;
  std::string m_game_id;
  u8 m_game_revision;

  CheatWarningWidget* m_warning;
  QListWidget* m_code_list;
  QLabel* m_name_label;
  QLabel* m_creator_label;
  QTextEdit* m_code_description;
  QTextEdit* m_code_view;
  QPushButton* m_download_codes;
  std::vector<Gecko::GeckoCode> m_gecko_codes;
};
