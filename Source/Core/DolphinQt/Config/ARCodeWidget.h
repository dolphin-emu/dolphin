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

class CheatWarningWidget;
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
  void SortEnabledCodesFirst();
  void SortDisabledCodesFirst();

  void OnCodeAddClicked();
  void OnCodeEditClicked();
  void OnCodeRemoveClicked();

  void OnListReordered();

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
