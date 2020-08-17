// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>
#include <QTableView>

#include "DolphinQt/Scripting/ScriptsListModel.h"

class ScriptingWidget : public QDockWidget
{
  Q_OBJECT
public:
  ScriptingWidget(QWidget* parent = nullptr);
  void AddNewScript();
  void ReloadSelectedScript();
  void RemoveSelectedScript();
  void AddScript(std::string filename);

private:
  ScriptsListModel* m_scripts_model;
  QTableView* m_table_view;
};
