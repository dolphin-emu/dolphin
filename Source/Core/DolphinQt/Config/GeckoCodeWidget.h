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
class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QPushButton;

namespace UICommon
{
class GameFile;
}

class GeckoCodeWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GeckoCodeWidget(const UICommon::GameFile& game, bool restart_required = true);

signals:
  void OpenGeneralSettings();

private:
  void OnSelectionChanged();
  void OnItemChanged(QListWidgetItem* item);
  void OnListReordered();
  void OnContextMenuRequested();

  void CreateWidgets();
  void ConnectWidgets();
  void UpdateList();
  void AddCode();
  void EditCode();
  void RemoveCode();
  void DownloadCodes();
  void SaveCodes();
  void SortAlphabetically();

  const UICommon::GameFile& m_game;
  std::string m_game_id;
  std::string m_gametdb_id;
  u16 m_game_revision;

  CheatWarningWidget* m_warning;
  QListWidget* m_code_list;
  QLabel* m_name_label;
  QLabel* m_creator_label;
  QTextEdit* m_code_description;
  QTextEdit* m_code_view;
  QPushButton* m_add_code;
  QPushButton* m_edit_code;
  QPushButton* m_remove_code;
  QPushButton* m_download_codes;
  std::vector<Gecko::GeckoCode> m_gecko_codes;
  bool m_restart_required;
};
