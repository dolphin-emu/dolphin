// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ARCodeWidget.h"

#include <QButtonGroup>
#include <QCursor>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatWarningWidget.h"

#include "UICommon/GameFile.h"

ARCodeWidget::ARCodeWidget(const UICommon::GameFile& game, bool restart_required)
    : m_game(game), m_game_id(game.GetGameID()), m_game_revision(game.GetRevision()),
      m_restart_required(restart_required)
{
  CreateWidgets();
  ConnectWidgets();

  IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  IniFile game_ini_default = SConfig::GetInstance().LoadDefaultGameIni(m_game_id, m_game_revision);
  m_ar_codes = ActionReplay::LoadCodes(game_ini_default, game_ini_local);

  UpdateList();
  OnSelectionChanged();
}

void ARCodeWidget::CreateWidgets()
{
  m_warning = new CheatWarningWidget(m_game_id, m_restart_required, this);
  m_code_list = new QListWidget;
  m_code_add = new QPushButton(tr("&Add New Code..."));
  m_code_edit = new QPushButton(tr("&Edit Code..."));
  m_code_remove = new QPushButton(tr("&Remove Code"));

  m_code_list->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* button_layout = new QHBoxLayout;

  button_layout->addWidget(m_code_add);
  button_layout->addWidget(m_code_edit);
  button_layout->addWidget(m_code_remove);

  QVBoxLayout* layout = new QVBoxLayout;

  layout->addWidget(m_warning);
  layout->addWidget(m_code_list);
  layout->addLayout(button_layout);

  setLayout(layout);
}

void ARCodeWidget::ConnectWidgets()
{
  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this,
          &ARCodeWidget::OpenGeneralSettings);

  connect(m_code_list, &QListWidget::itemChanged, this, &ARCodeWidget::OnItemChanged);
  connect(m_code_list, &QListWidget::itemSelectionChanged, this, &ARCodeWidget::OnSelectionChanged);
  connect(m_code_list->model(), &QAbstractItemModel::rowsMoved, this,
          &ARCodeWidget::OnListReordered);
  connect(m_code_list, &QListWidget::customContextMenuRequested, this,
          &ARCodeWidget::OnContextMenuRequested);

  connect(m_code_add, &QPushButton::pressed, this, &ARCodeWidget::OnCodeAddPressed);
  connect(m_code_edit, &QPushButton::pressed, this, &ARCodeWidget::OnCodeEditPressed);
  connect(m_code_remove, &QPushButton::pressed, this, &ARCodeWidget::OnCodeRemovePressed);
}

void ARCodeWidget::OnItemChanged(QListWidgetItem* item)
{
  m_ar_codes[m_code_list->row(item)].active = (item->checkState() == Qt::Checked);

  if (!m_restart_required)
    ActionReplay::ApplyCodes(m_ar_codes);

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::OnContextMenuRequested()
{
  QMenu menu;

  menu.addAction(tr("Sort Alphabetically"), this, &ARCodeWidget::SortAlphabetically);

  menu.exec(QCursor::pos());
}

void ARCodeWidget::SortAlphabetically()
{
  m_code_list->sortItems();
  OnListReordered();
}

void ARCodeWidget::OnListReordered()
{
  // Reorder codes based on the indices of table item
  std::vector<ActionReplay::ARCode> codes;
  codes.reserve(m_ar_codes.size());

  for (int i = 0; i < m_code_list->count(); i++)
  {
    const int index = m_code_list->item(i)->data(Qt::UserRole).toInt();

    codes.push_back(std::move(m_ar_codes[index]));
  }

  m_ar_codes = std::move(codes);

  SaveCodes();
}

void ARCodeWidget::OnSelectionChanged()
{
  auto items = m_code_list->selectedItems();

  if (items.empty())
    return;

  const auto* selected = items[0];

  bool user_defined = m_ar_codes[m_code_list->row(selected)].user_defined;

  m_code_remove->setEnabled(user_defined);
  m_code_edit->setText(user_defined ? tr("&Edit Code...") : tr("Clone and &Edit Code..."));
}

void ARCodeWidget::UpdateList()
{
  m_code_list->clear();

  for (size_t i = 0; i < m_ar_codes.size(); i++)
  {
    const auto& ar = m_ar_codes[i];
    auto* item = new QListWidgetItem(QString::fromStdString(ar.name)
                                         .replace(QStringLiteral("&lt;"), QStringLiteral("<"))
                                         .replace(QStringLiteral("&gt;"), QStringLiteral(">")));

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                   Qt::ItemIsDragEnabled);
    item->setCheckState(ar.active ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, static_cast<int>(i));

    m_code_list->addItem(item);
  }

  m_code_list->setDragDropMode(QAbstractItemView::InternalMove);
}

void ARCodeWidget::SaveCodes()
{
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
  ActionReplay::SaveCodes(&game_ini_local, m_ar_codes);

  game_ini_local.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");
}

void ARCodeWidget::AddCode(ActionReplay::ARCode code)
{
  m_ar_codes.push_back(std::move(code));

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::OnCodeAddPressed()
{
  ActionReplay::ARCode ar;
  ar.active = true;

  CheatCodeEditor ed(this);

  ed.SetARCode(&ar);

  if (ed.exec())
  {
    m_ar_codes.push_back(std::move(ar));

    UpdateList();
    SaveCodes();
  }
}

void ARCodeWidget::OnCodeEditPressed()
{
  auto items = m_code_list->selectedItems();

  if (items.empty())
    return;

  const auto* selected = items[0];

  auto& current_ar = m_ar_codes[m_code_list->row(selected)];

  bool user_defined = current_ar.user_defined;

  ActionReplay::ARCode ar = current_ar;

  CheatCodeEditor ed(this);

  ed.SetARCode(user_defined ? &current_ar : &ar);
  ed.exec();

  if (!user_defined)
    m_ar_codes.push_back(ar);

  SaveCodes();
  UpdateList();
}

void ARCodeWidget::OnCodeRemovePressed()
{
  auto items = m_code_list->selectedItems();

  if (items.empty())
    return;

  const auto* selected = items[0];

  m_ar_codes.erase(m_ar_codes.begin() + m_code_list->row(selected));

  SaveCodes();
  UpdateList();

  m_code_remove->setEnabled(false);
}
