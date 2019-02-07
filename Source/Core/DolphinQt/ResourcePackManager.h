// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class ResourcePackManager : public QDialog
{
public:
  explicit ResourcePackManager(QWidget* parent = nullptr);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void OpenResourcePackDir();
  void RepopulateTable();
  void Change();
  void Install();
  void Uninstall();
  void Remove();
  void PriorityUp();
  void PriorityDown();
  void Refresh();

  void SelectionChanged();
  void ItemDoubleClicked(QTableWidgetItem* item);

  int GetResourcePackIndex(QTableWidgetItem* item) const;

  QPushButton* m_open_directory_button;
  QPushButton* m_change_button;
  QPushButton* m_remove_button;
  QPushButton* m_refresh_button;
  QPushButton* m_priority_up_button;
  QPushButton* m_priority_down_button;

  QTableWidget* m_table_widget;
};
