// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace ActionReplay
{
struct ARCode;
}

class CheatCodeEditor;
class CheatWarningWidget;
#ifdef USE_RETRO_ACHIEVEMENTS
class HardcoreWarningWidget;
#endif  // USE_RETRO_ACHIEVEMENTS
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class ARCodeWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ARCodeWidget(std::string game_id, u16 game_revision, bool restart_required = true);
  ~ARCodeWidget() override;

  void ChangeGame(std::string game_id, u16 game_revision);
  void AddCode(ActionReplay::ARCode code);

signals:
  void OpenGeneralSettings();
#ifdef USE_RETRO_ACHIEVEMENTS
  void OpenAchievementSettings();
#endif  // USE_RETRO_ACHIEVEMENTS

private:
  void OnSelectionChanged();
  void OnItemChanged(QListWidgetItem* item);
  void OnContextMenuRequested();

  void CreateWidgets();
  void ConnectWidgets();
  void UpdateList();
  void LoadCodes();
  void SaveCodes();
  void SortAlphabetically();
  void SortEnabledCodesFirst();
  void SortDisabledCodesFirst();

  void OnCodeAddClicked();
  void OnCodeEditClicked();
  void OnCodeRemoveClicked();

  void OnListReordered();

  std::string m_game_id;
  u16 m_game_revision;

  CheatWarningWidget* m_warning;
#ifdef USE_RETRO_ACHIEVEMENTS
  HardcoreWarningWidget* m_hc_warning;
#endif  // USE_RETRO_ACHIEVEMENTS
  QListWidget* m_code_list;
  QPushButton* m_code_add;
  QPushButton* m_code_edit;
  QPushButton* m_code_remove;

  CheatCodeEditor* m_cheat_code_editor;

  std::vector<ActionReplay::ARCode> m_ar_codes;
  bool m_restart_required;
};
