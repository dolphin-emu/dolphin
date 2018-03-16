// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include <QWidget>

#include "Core/PatchEngine.h"
#include "UICommon/GameFile.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;

class PatchesWidget : public QWidget
{
public:
  explicit PatchesWidget(const UICommon::GameFile& game);

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

  QListWidget* m_list;
  QPushButton* m_add_button;
  QPushButton* m_edit_button;
  QPushButton* m_remove_button;

  std::vector<PatchEngine::Patch> m_patches;
  const UICommon::GameFile& m_game;
  std::string m_game_id;
  u16 m_game_revision;
};
