// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ScriptsWidget.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/ScriptEngine.h"

#include "UICommon/GameFile.h"

ScriptsWidget::ScriptsWidget(const UICommon::GameFile& game) : m_game_id(game.GetGameID())
{
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
  ScriptEngine::LoadScriptSection("Scripts", m_scripts, game_ini_local);

  CreateWidgets();
  ConnectWidgets();

  Update();

  UpdateActions();
}

ScriptsWidget::~ScriptsWidget() = default;

void ScriptsWidget::CreateWidgets()
{
  m_list = new QListWidget;
  m_add_button = new QPushButton(tr("&Add..."));
  m_remove_button = new QPushButton(tr("&Remove"));

  auto* layout = new QGridLayout;

  layout->addWidget(m_list, 0, 0, 1, -1);
  layout->addWidget(m_add_button, 1, 0);
  layout->addWidget(m_remove_button, 1, 1);

  setLayout(layout);
}

void ScriptsWidget::ConnectWidgets()
{
  connect(m_list, &QListWidget::itemSelectionChanged, this, &ScriptsWidget::UpdateActions);
  connect(m_list, &QListWidget::itemChanged, this, &ScriptsWidget::OnItemChanged);
  connect(m_remove_button, &QPushButton::clicked, this, &ScriptsWidget::OnRemove);
  connect(m_add_button, &QPushButton::clicked, this, &ScriptsWidget::OnAdd);
}

void ScriptsWidget::OnItemChanged(QListWidgetItem* item)
{
  m_scripts[m_list->row(item)].active = (item->checkState() == Qt::Checked);
  SaveScripts();
}

void ScriptsWidget::OnAdd()
{
  QString path = QFileDialog::getOpenFileName(this, tr("Select a script"), QDir::currentPath(),
                                              tr("Lua script (*.lua);; All Files (*)"));
  if (!path.isEmpty())
  {
    ScriptEngine::ScriptTarget script{.file_path = path.toStdString(), .active = true};
    m_scripts.push_back(script);
    SaveScripts();
    Update();
  }
}

void ScriptsWidget::OnRemove()
{
  if (m_list->selectedItems().isEmpty())
    return;
  m_scripts.erase(m_scripts.begin() + m_list->row(m_list->selectedItems()[0]));
  SaveScripts();
  Update();
}

void ScriptsWidget::SaveScripts()
{
  std::vector<std::string> lines;

  for (const auto& script : m_scripts)
  {
    if (script.active)
      lines.push_back("y " + script.file_path);
    else
      lines.push_back("n " + script.file_path);
  }

  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
  game_ini_local.SetLines("Scripts", lines);
  game_ini_local.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
}

void ScriptsWidget::Update()
{
  m_list->clear();

  for (const auto& script : m_scripts)
  {
    auto* item = new QListWidgetItem(QString::fromStdString(script.file_path));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(script.active ? Qt::Checked : Qt::Unchecked);

    m_list->addItem(item);
  }
}

void ScriptsWidget::UpdateActions()
{
  bool selected = !m_list->selectedItems().isEmpty();
  m_remove_button->setEnabled(selected);
}
