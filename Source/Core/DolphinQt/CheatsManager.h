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
class CheatSearchFactoryWidget;
class QDialogButtonBox;
class PartiallyClosableTabWidget;

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

signals:
  void OpenGeneralSettings();
  void ShowMemory(u32 address);
  void RequestWatch(QString name, u32 address);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void OnStateChanged(Core::State state);
  void OnNewSessionCreated(const Cheats::CheatSearchSessionBase& session);
  void OnTabCloseRequested(int index);

  void RefreshCodeTabs(Core::State state, bool force);

  std::string m_game_id;
  std::string m_game_tdb_id;
  u16 m_revision = 0;

  QDialogButtonBox* m_button_box;
  PartiallyClosableTabWidget* m_tab_widget = nullptr;

  ARCodeWidget* m_ar_code = nullptr;
  GeckoCodeWidget* m_gecko_code = nullptr;
  CheatSearchFactoryWidget* m_cheat_search_new = nullptr;
};
