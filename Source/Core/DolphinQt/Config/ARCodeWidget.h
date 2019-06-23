// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/ActionReplay.h"

namespace UICommon
{
class GameFile;
}

class CheatWarningWidget;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class ARCodeWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ARCodeWidget(const UICommon::GameFile& game, bool restart_required = true);

  void AddCode(ActionReplay::ARCode code);

signals:
  void OpenGeneralSettings();

private:
  void OnSelectionChanged();
  void OnItemChanged(QListWidgetItem* item);
  void OnContextMenuRequested();

  void CreateWidgets();
  void ConnectWidgets();
  void UpdateList();
  void SaveCodes();
  void SortAlphabetically();

  void OnCodeAddPressed();
  void OnCodeEditPressed();
  void OnCodeRemovePressed();

  void OnListReordered();

  const UICommon::GameFile& m_game;
  std::string m_game_id;
  u16 m_game_revision;

  CheatWarningWidget* m_warning;
  QListWidget* m_code_list;
  QPushButton* m_code_add;
  QPushButton* m_code_edit;
  QPushButton* m_code_remove;

  std::vector<ActionReplay::ARCode> m_ar_codes;
  bool m_restart_required;
};
