// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/PatchesWidget.h"

#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/PatchEngine.h"

#include "DolphinQt/Config/NewPatchDialog.h"

#include "UICommon/GameFile.h"

PatchesWidget::PatchesWidget(const UICommon::GameFile& game)
    : m_game_id(game.GetGameID()), m_game_revision(game.GetRevision())
{
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  IniFile game_ini_default = SConfig::GetInstance().LoadDefaultGameIni(m_game_id, m_game_revision);

  PatchEngine::LoadPatchSection("OnFrame", m_patches, game_ini_default, game_ini_local);

  CreateWidgets();
  ConnectWidgets();

  Update();

  UpdateActions();
}

void PatchesWidget::CreateWidgets()
{
  m_list = new QListWidget;
  m_add_button = new QPushButton(tr("&Add..."));
  m_edit_button = new QPushButton();
  m_remove_button = new QPushButton(tr("&Remove"));

  auto* layout = new QGridLayout;

  layout->addWidget(m_list, 0, 0, 1, -1);
  layout->addWidget(m_add_button, 1, 0);
  layout->addWidget(m_edit_button, 1, 2);
  layout->addWidget(m_remove_button, 1, 1);

  setLayout(layout);
}

void PatchesWidget::ConnectWidgets()
{
  connect(m_list, &QListWidget::itemSelectionChanged, this, &PatchesWidget::UpdateActions);
  connect(m_list, &QListWidget::itemChanged, this, &PatchesWidget::OnItemChanged);
  connect(m_remove_button, &QPushButton::pressed, this, &PatchesWidget::OnRemove);
  connect(m_add_button, &QPushButton::pressed, this, &PatchesWidget::OnAdd);
  connect(m_edit_button, &QPushButton::pressed, this, &PatchesWidget::OnEdit);
}

void PatchesWidget::OnItemChanged(QListWidgetItem* item)
{
  m_patches[m_list->row(item)].active = (item->checkState() == Qt::Checked);
  SavePatches();
}

void PatchesWidget::OnAdd()
{
  PatchEngine::Patch patch;
  patch.user_defined = true;

  if (NewPatchDialog(this, patch).exec())
  {
    m_patches.push_back(patch);
    SavePatches();
    Update();
  }
}

void PatchesWidget::OnEdit()
{
  if (m_list->selectedItems().isEmpty())
    return;

  auto* item = m_list->selectedItems()[0];

  auto patch = m_patches[m_list->row(item)];

  if (!patch.user_defined)
  {
    // i18n: If there is a pre-defined patch with the name %1 and the user wants to edit it,
    // a copy of it gets created with this name
    patch.name = tr("%1 (Copy)").arg(QString::fromStdString(patch.name)).toStdString();
  }

  if (NewPatchDialog(this, patch).exec())
  {
    if (patch.user_defined)
    {
      m_patches[m_list->row(item)] = patch;
    }
    else
    {
      patch.user_defined = true;
      m_patches.push_back(patch);
    }

    SavePatches();
    Update();
  }
}

void PatchesWidget::OnRemove()
{
  if (m_list->selectedItems().isEmpty())
    return;

  m_patches.erase(m_patches.begin() + m_list->row(m_list->selectedItems()[0]));
  SavePatches();
  Update();
}

void PatchesWidget::SavePatches()
{
  std::vector<std::string> lines;
  std::vector<std::string> lines_enabled;

  for (const auto& patch : m_patches)
  {
    if (patch.active)
      lines_enabled.push_back("$" + patch.name);

    if (!patch.user_defined)
      continue;

    lines.push_back("$" + patch.name);

    for (const auto& entry : patch.entries)
    {
      lines.push_back(StringFromFormat("0x%08X:%s:0x%08X", entry.address,
                                       PatchEngine::PatchTypeAsString(entry.type), entry.value));
    }
  }

  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  game_ini_local.SetLines("OnFrame_Enabled", lines_enabled);
  game_ini_local.SetLines("OnFrame", lines);

  game_ini_local.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
}

void PatchesWidget::Update()
{
  m_list->clear();

  for (const auto& patch : m_patches)
  {
    auto* item = new QListWidgetItem(QString::fromStdString(patch.name));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(patch.active ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, patch.user_defined);

    m_list->addItem(item);
  }
}

void PatchesWidget::UpdateActions()
{
  bool selected = !m_list->selectedItems().isEmpty();

  auto* item = selected ? m_list->selectedItems()[0] : nullptr;

  bool user_defined = selected ? item->data(Qt::UserRole).toBool() : true;

  m_edit_button->setEnabled(selected);
  m_edit_button->setText(user_defined ? tr("&Edit...") : tr("&Clone..."));
  m_remove_button->setEnabled(selected && user_defined);
}
