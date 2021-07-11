// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"
#include "DolphinQt/GameList/GameListModel.h"

#include "Core/CheatSearch.h"

class ARCodeWidget;
class GeckoCodeWidget;
class CheatSearchNewWidget;
class QDialogButtonBox;
class QTabWidget;

namespace Core
{
enum class State;
}

class CheatsManager : public QDialog
{
  Q_OBJECT
public:
  explicit CheatsManager(QWidget* parent = nullptr);
  ~CheatsManager();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void OnStateChanged(Core::State state);

  std::string m_game_id;
  std::string m_game_tdb_id;
  u16 m_revision = 0;

  std::vector<Cheats::SearchResult> m_results;
  QDialogButtonBox* m_button_box;
  QTabWidget* m_tab_widget = nullptr;

  ARCodeWidget* m_ar_code = nullptr;
  GeckoCodeWidget* m_gecko_code = nullptr;
  CheatSearchNewWidget* m_cheat_search_new = nullptr;
};
