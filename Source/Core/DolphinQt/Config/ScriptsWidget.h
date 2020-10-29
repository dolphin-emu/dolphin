// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace ScriptEngine
{
struct ScriptTarget;
}

class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace UICommon
{
class GameFile;
}

class ScriptsWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ScriptsWidget(const UICommon::GameFile& game);
  ~ScriptsWidget() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void SaveScripts();
  void Update();
  void UpdateActions();

  void OnItemChanged(QListWidgetItem*);
  void OnAdd();
  void OnRemove();

  QListWidget* m_list;
  QPushButton* m_add_button;
  QPushButton* m_remove_button;

  std::vector<ScriptEngine::ScriptTarget> m_scripts;
  std::string m_game_id;
};
