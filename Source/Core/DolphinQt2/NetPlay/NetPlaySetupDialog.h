// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class GameListModel;
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

class NetPlaySetupDialog : public QDialog
{
  Q_OBJECT
public:
  NetPlaySetupDialog(QWidget* parent);

  void accept() override;
  void show();

signals:
  bool Join();
  bool Host(const QString& game_identifier);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void PopulateGameList();
  void ResetTraversalHost();

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
  QLabel* m_host_port_label;
  QSpinBox* m_host_port_box;
  QListWidget* m_host_games;
  QPushButton* m_host_button;
  QCheckBox* m_host_force_port_check;
  QSpinBox* m_host_force_port_box;

#ifdef USE_UPNP
  QCheckBox* m_host_upnp;
#endif

  GameListModel* m_game_list_model;
};
