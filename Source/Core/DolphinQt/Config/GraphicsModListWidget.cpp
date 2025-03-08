// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GraphicsModListWidget.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <set>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "DolphinQt/Config/GraphicsModWarningWidget.h"
#include "DolphinQt/QtUtils/ClearLayoutRecursively.h"
#include "DolphinQt/Settings.h"
#include "UICommon/GameFile.h"
#include "VideoCommon/VideoConfig.h"

GraphicsModListWidget::GraphicsModListWidget(const UICommon::GameFile& game)
    : m_game_id(game.GetGameID()), m_mod_group(m_game_id)
{
  CalculateGameRunning(Core::GetState(Core::System::GetInstance()));
  if (m_loaded_game_is_running && g_Config.graphics_mod_config)
  {
    m_mod_group.SetChangeCount(g_Config.graphics_mod_config->GetChangeCount());
  }
  CreateWidgets();
  ConnectWidgets();

  RefreshModList();
  OnModChanged(0);
}

GraphicsModListWidget::~GraphicsModListWidget()
{
  if (m_needs_save)
  {
    m_mod_group.Save();
  }
}

void GraphicsModListWidget::CreateWidgets()
{
  auto* main_v_layout = new QVBoxLayout(this);

  auto* main_layout = new QHBoxLayout;

  auto* left_v_layout = new QVBoxLayout;

  m_mod_list = new QListWidget;
  m_mod_list->setSortingEnabled(false);
  m_mod_list->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectItems);
  m_mod_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  m_mod_list->setSelectionRectVisible(true);
  m_mod_list->setDragDropMode(QAbstractItemView::InternalMove);

  m_open_directory_button = new QPushButton(tr("Open Directory..."));
  m_refresh = new QPushButton(tr("&Refresh List"));
  QHBoxLayout* hlayout = new QHBoxLayout;
  hlayout->addWidget(m_open_directory_button);
  hlayout->addWidget(m_refresh);

  left_v_layout->addWidget(m_mod_list);
  left_v_layout->addLayout(hlayout);

  auto* right_v_layout = new QVBoxLayout;

  m_selected_mod_name = new QLabel();
  right_v_layout->addWidget(m_selected_mod_name);

  m_mod_meta_layout = new QVBoxLayout;
  right_v_layout->addLayout(m_mod_meta_layout);
  right_v_layout->addStretch();

  main_layout->addLayout(left_v_layout);
  main_layout->addLayout(right_v_layout, 1);

  m_warning = new GraphicsModWarningWidget(this);
  main_v_layout->addWidget(m_warning);
  main_v_layout->addLayout(main_layout);

  setLayout(main_v_layout);
}

void GraphicsModListWidget::ConnectWidgets()
{
  connect(m_warning, &GraphicsModWarningWidget::GraphicsModEnableSettings, this,
          &GraphicsModListWidget::OpenGraphicsSettings);

  connect(m_mod_list, &QListWidget::itemSelectionChanged, this,
          &GraphicsModListWidget::ModSelectionChanged);

  connect(m_mod_list, &QListWidget::itemChanged, this, &GraphicsModListWidget::ModItemChanged);

  connect(m_mod_list->model(), &QAbstractItemModel::rowsMoved, this,
          &GraphicsModListWidget::SaveModList);

  connect(m_open_directory_button, &QPushButton::clicked, this,
          &GraphicsModListWidget::OpenGraphicsModDir);

  connect(m_refresh, &QPushButton::clicked, this, &GraphicsModListWidget::RefreshModList);

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &GraphicsModListWidget::CalculateGameRunning);
}

void GraphicsModListWidget::RefreshModList()
{
  if (m_needs_save)
  {
    SaveToDisk();
  }

  m_mod_list->setCurrentItem(nullptr);
  m_mod_list->clear();

  m_mod_group = GraphicsModSystem::Config::GraphicsModGroup(m_game_id);
  m_mod_group.Load();

  for (const auto& mod : m_mod_group.GetMods())
  {
    if (mod.m_mod.m_actions.empty())
      continue;

    QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(mod.m_mod.m_title));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setData(Qt::UserRole, static_cast<qulonglong>(mod.m_id));
    item->setCheckState(mod.m_enabled ? Qt::Checked : Qt::Unchecked);

    m_mod_list->addItem(item);
  }
}

void GraphicsModListWidget::ModSelectionChanged()
{
  if (m_mod_list->currentItem() == nullptr)
    return;
  if (m_mod_list->count() == 0)
    return;
  OnModChanged(m_mod_list->currentItem()->data(Qt::UserRole).toULongLong());
}

void GraphicsModListWidget::ModItemChanged(QListWidgetItem* item)
{
  const auto id = item->data(Qt::UserRole).toULongLong();
  auto mod = m_mod_group.GetMod(id);
  if (!mod)
    return;

  const bool was_enabled = mod->m_enabled;
  const bool should_enable = item->checkState() == Qt::Checked;
  mod->m_enabled = should_enable;
  if (was_enabled == should_enable)
    return;

  m_mod_group.SetChangeCount(m_mod_group.GetChangeCount() + 1);
  if (m_loaded_game_is_running)
  {
    g_Config.graphics_mod_config = m_mod_group;
  }
  m_needs_save = true;
}

void GraphicsModListWidget::OnModChanged(u64 id)
{
  ClearLayoutRecursively(m_mod_meta_layout);

  adjustSize();

  if (id == 0)
  {
    m_selected_mod_name->setText(tr("No graphics mod selected"));
    m_selected_mod_name->setAlignment(Qt::AlignCenter);
    return;
  }

  const auto mod = m_mod_group.GetMod(id);
  if (!mod)
    return;

  m_selected_mod_name->setText(QString::fromStdString(mod->m_mod.m_title));
  m_selected_mod_name->setAlignment(Qt::AlignLeft);
  QFont font = m_selected_mod_name->font();
  font.setWeight(QFont::Bold);
  m_selected_mod_name->setFont(font);

  if (!mod->m_mod.m_author.empty())
  {
    auto* author_label = new QLabel(tr("By: %1").arg(QString::fromStdString(mod->m_mod.m_author)));
    m_mod_meta_layout->addWidget(author_label);
  }

  if (!mod->m_mod.m_description.empty())
  {
    auto* description_label =
        new QLabel(tr("Description: %1").arg(QString::fromStdString(mod->m_mod.m_description)));
    description_label->setWordWrap(true);
    m_mod_meta_layout->addWidget(description_label);
  }
}

void GraphicsModListWidget::SaveModList()
{
  for (int i = 0; i < m_mod_list->count(); i++)
  {
    const auto id =
        m_mod_list->model()->data(m_mod_list->model()->index(i, 0), Qt::UserRole).toULongLong();
    m_mod_group.GetMod(id)->m_weight = i;
  }

  if (m_loaded_game_is_running)
  {
    g_Config.graphics_mod_config = m_mod_group;
  }
  m_needs_save = true;
}

void GraphicsModListWidget::SaveToDisk()
{
  m_needs_save = false;
  m_mod_group.Save();
}

const GraphicsModSystem::Config::GraphicsModGroup&
GraphicsModListWidget::GetGraphicsModConfig() const
{
  return m_mod_group;
}

void GraphicsModListWidget::CalculateGameRunning(Core::State state)
{
  m_loaded_game_is_running =
      state == Core::State::Running ? m_game_id == SConfig::GetInstance().GetGameID() : false;
}

void GraphicsModListWidget::OpenGraphicsModDir()
{
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdString(File::GetUserPath(D_GRAPHICSMOD_IDX))));
}
