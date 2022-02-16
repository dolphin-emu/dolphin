// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "DolphinQt/GameList/GameListModel.h"
#include "UICommon/NetPlayIndex.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QGridLayout;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QGroupBox;
class QTableWidget;
class QRadioButton;

namespace UICommon
{
class GameFile;
}

class NetPlaySetupDialog : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent);
  ~NetPlaySetupDialog();

  void accept() override;
  void show();

signals:
  bool Join();
  void JoinBrowser();
  bool Host(const UICommon::GameFile& game);
  void UpdateStatusRequestedBrowser(const QString& status);
  void UpdateListRequestedBrowser(std::vector<NetPlaySession> sessions);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void PopulateGameList();
  void ResetTraversalHost();

  // Browser Stuff
  void RefreshBrowser();
  void RefreshLoopBrowser();
  void UpdateListBrowser();
  void OnSelectionChangedBrowser();
  void OnUpdateStatusRequestedBrowser(const QString& status);
  void OnUpdateListRequestedBrowser(std::vector<NetPlaySession> sessions);
  void acceptBrowser();

  void SaveSettings();

  void OnConnectionTypeChanged(int index);

  // Main Widget
  QDialogButtonBox* m_button_box;
  QComboBox* m_connection_type;
  QLineEdit* m_nickname_edit;
  QGridLayout* m_main_layout;
  QTabWidget* m_tab_widget;
  QPushButton* m_reset_traversal_button;

  // Connection Widget
  QLabel* m_ip_label;
  QLineEdit* m_ip_edit;
  QLabel* m_connect_port_label;
  QSpinBox* m_connect_port_box;
  QPushButton* m_connect_button;

  // Host Widget
  QSpinBox* m_host_port_box;
  QListWidget* m_host_games;
  QPushButton* m_host_button;
  QCheckBox* m_host_chunked_upload_limit_check;
  QSpinBox* m_host_chunked_upload_limit_box;
  QCheckBox* m_host_server_browser;
  QLineEdit* m_host_server_name;

  QTableWidget* m_table_widget;
  QComboBox* m_region_combo;
  QLabel* m_status_label;
  QDialogButtonBox* m_b_button_box;
  QPushButton* m_button_refresh;
  QLineEdit* m_edit_name;
  QCheckBox* m_check_hide_ingame;
  QRadioButton* m_radio_all;
  QRadioButton* m_radio_private;
  QRadioButton* m_radio_public;

  std::vector<NetPlaySession> m_sessions;

  std::thread m_refresh_thread;
  std::optional<std::map<std::string, std::string>> m_refresh_filters;
  std::mutex m_refresh_filters_mutex;
  Common::Flag m_refresh_run;
  Common::Event m_refresh_event;

#ifdef USE_UPNP
  QCheckBox* m_host_upnp;
#endif

  const GameListModel& m_game_list_model;
};
