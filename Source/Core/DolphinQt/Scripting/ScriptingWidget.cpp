// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QWidget>
#include <QString>
#include <QListView>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>

#include "Common/MsgHandler.h"
#include "DolphinQt/Scripting/ScriptingWidget.h"
#include "DolphinQt/Settings.h"

ScriptingWidget::ScriptingWidget(QWidget* parent)
{
  setWindowTitle(tr("Scripts"));

  // actions
  QPushButton* button_add_new = new QPushButton(tr("Add New Script"));
  QPushButton* button_reload_selected = new QPushButton(tr("Reload Selected Script"));
  QPushButton* button_remove_selected = new QPushButton(tr("Remove Selected Script"));
  QHBoxLayout* actions_layout = new QHBoxLayout;
  actions_layout->addWidget(button_add_new);
  actions_layout->addWidget(button_reload_selected);
  actions_layout->addWidget(button_remove_selected);
  QWidget* actions_widget = new QWidget;
  actions_widget->setLayout(actions_layout);

  // table view
  m_scripts_model = new ScriptsListModel();
  m_table_view = new QTableView();
  m_table_view->setModel(m_scripts_model);
  m_table_view->horizontalHeader()->setStretchLastSection(true);
  m_table_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  m_table_view->verticalHeader()->hide();
  m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_view->setSelectionMode(QAbstractItemView::SingleSelection);

  // put in group box
  QGroupBox* scripts_group = new QGroupBox(tr("Running scripts"));
  QVBoxLayout* scripts_layout = new QVBoxLayout;
  scripts_group->setLayout(scripts_layout);
  scripts_layout->addWidget(m_table_view);

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addWidget(actions_widget);
  main_layout->addWidget(scripts_group);
  QWidget* main_widget = new QWidget;
  main_widget->setLayout(main_layout);
  this->setWidget(main_widget);

  connect(&Settings::Instance(), &Settings::ScriptingVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(button_add_new, &QPushButton::clicked, this, &ScriptingWidget::AddNewScript);
  connect(button_reload_selected, &QPushButton::clicked, this,
          &ScriptingWidget::ReloadSelectedScript);
  connect(button_remove_selected, &QPushButton::clicked, this,
          &ScriptingWidget::RemoveSelectedScript);
}

void ScriptingWidget::AddNewScript()
{
  if (Scripting::ScriptingBackend::PythonSubinterpretersDisabled() && m_scripts_model->rowCount() == 1)
  {
    CriticalAlertFmt("Can run at most one script at a time because Python subinterpreters are disabled");
    return;
  }

  QString filename = QFileDialog::getOpenFileName(
      this, tr("Add script"), QString(),
      tr("Python scripts (*.py *.py3)"));

  if (!filename.isEmpty())
  {
    m_scripts_model->Add(filename.toStdString());
  }
}

void ScriptingWidget::ReloadSelectedScript()
{
  for (const QModelIndex& q_index : m_table_view->selectionModel()->selectedRows())
  {
    m_scripts_model->Reload(q_index.row());
  }
}

void ScriptingWidget::RemoveSelectedScript()
{
  for (const QModelIndex& q_index : m_table_view->selectionModel()->selectedRows())
  {
    m_scripts_model->Remove(q_index.row());
  }
}

void ScriptingWidget::AddScript(std::string filename)
{
  m_scripts_model->Add(filename);
}
