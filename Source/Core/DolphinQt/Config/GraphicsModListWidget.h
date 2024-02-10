// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <QWidget>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"

class GraphicsModWarningWidget;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QModelIndex;
class QPushButton;
class QVBoxLayout;

namespace Core
{
enum class State;
}

namespace UICommon
{
class GameFile;
}

class GraphicsModListWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GraphicsModListWidget(const UICommon::GameFile& game);
  ~GraphicsModListWidget();

  void SaveToDisk();

  const GraphicsModGroupConfig& GetGraphicsModConfig() const;

signals:
  void OpenGraphicsSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void RefreshModList();
  void ModSelectionChanged();
  void ModItemChanged(QListWidgetItem* item);

  void OnModChanged(const std::optional<std::string>& absolute_path);

  void SaveModList();

  void OpenGraphicsModDir();

  void CalculateGameRunning(Core::State state);
  bool m_loaded_game_is_running = false;
  bool m_needs_save = false;

  QListWidget* m_mod_list;

  QLabel* m_selected_mod_name;
  QVBoxLayout* m_mod_meta_layout;

  QPushButton* m_open_directory_button;
  QPushButton* m_refresh;
  GraphicsModWarningWidget* m_warning;

  std::string m_game_id;
  GraphicsModGroupConfig m_mod_group;
};
