// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include <QDockWidget>

#include "Common/CommonTypes.h"
#include "DolphinQt/Debugger/RegisterColumn.h"

class QTableWidget;
class QCloseEvent;
class QShowEvent;

class RegisterWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit RegisterWidget(QWidget* parent = nullptr);
  ~RegisterWidget();

signals:
  void RequestTableUpdate();
  void RequestMemoryBreakpoint(u32 addr);
  void UpdateTable();
  void UpdateValue(QTableWidgetItem* item);
  void UpdateValueType(QTableWidgetItem* item);

protected:
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateTable();

  void ShowContextMenu();
  void OnItemChanged(QTableWidgetItem* item);

  void AddRegister(int row, int column, RegisterType type, std::string register_name,
                   std::function<u64()> get_reg, std::function<void(u64)> set_reg);

  void Update();

  QTableWidget* m_table;
  bool m_updating = false;
};
