// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QDialog>

#include "UICommon/NetPlayIndex.h"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTableWidget;

class NetPlayBrowser : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlayBrowser(QWidget* parent = nullptr);

  void accept() override;
signals:
  void Join();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Refresh();

  void OnSelectionChanged();

  QComboBox* m_region_combo;
  QLabel* m_status_label;
  QPushButton* m_button_refresh;
  QTableWidget* m_table_widget;
  QDialogButtonBox* m_button_box;
  QLineEdit* m_edit_name;
  QLineEdit* m_edit_game_id;

  QRadioButton* m_radio_all;
  QRadioButton* m_radio_private;
  QRadioButton* m_radio_public;

  std::vector<NetPlaySession> m_sessions;
};
