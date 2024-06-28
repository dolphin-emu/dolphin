// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class CheatWarningWidget;
#ifdef USE_RETRO_ACHIEVEMENTS
class HardcoreWarningWidget;
#endif  // USE_RETRO_ACHIEVEMENTS
class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QPushButton;

namespace Gecko
{
class GeckoCode;
}

class GeckoCodeWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GeckoCodeWidget(std::string game_id, std::string gametdb_id, u16 game_revision);
  ~GeckoCodeWidget() override;

signals:
  void OpenGeneralSettings();
#ifdef USE_RETRO_ACHIEVEMENTS
  void OpenAchievementSettings();
#endif  // USE_RETRO_ACHIEVEMENTS

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
  void SortEnabledCodesFirst();
  void SortDisabledCodesFirst();

  std::string m_game_id;
  std::string m_gametdb_id;
  u16 m_game_revision;

  CheatWarningWidget* m_warning;
#ifdef USE_RETRO_ACHIEVEMENTS
  HardcoreWarningWidget* m_hc_warning;
#endif  // USE_RETRO_ACHIEVEMENTS
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
};
