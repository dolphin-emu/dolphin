// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <QWidget>

#include "Common/CommonTypes.h"
#include "Core/PatchEngine.h"

class HardcoreWarningWidget;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace UICommon
{
class GameFile;
}

class PatchesWidget : public QWidget
{
  Q_OBJECT
public:
  explicit PatchesWidget(const UICommon::GameFile& game);

signals:
  void OpenAchievementSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void SavePatches();
  void Update();
  void UpdateActions();

  void OnItemChanged(QListWidgetItem*);
  void OnAdd();
  void OnRemove();
  void OnEdit();

  HardcoreWarningWidget* m_hc_warning;
  QListWidget* m_list;
  QPushButton* m_add_button;
  QPushButton* m_edit_button;
  QPushButton* m_remove_button;

  std::vector<PatchEngine::Patch> m_patches;
  std::string m_game_id;
  u16 m_game_revision;
};
