// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ARCodeWidget.h"

#include <algorithm>
#include <utility>

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
#include "DolphinQt/Config/HardcoreWarningWidget.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include "UICommon/GameFile.h"

ARCodeWidget::ARCodeWidget(std::string game_id, u16 game_revision, bool restart_required)
    : m_game_id(std::move(game_id)), m_game_revision(game_revision),
      m_restart_required(restart_required)
{
  CreateWidgets();
  ConnectWidgets();

  if (!m_game_id.empty())
  {
    Common::IniFile game_ini_local;

    // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
    // will always be stored in GS/${GAMEID}.ini
    game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

    const Common::IniFile game_ini_default =
        SConfig::LoadDefaultGameIni(m_game_id, m_game_revision);
    m_ar_codes = ActionReplay::LoadCodes(game_ini_default, game_ini_local);
  }

  UpdateList();
  OnSelectionChanged();
}

ARCodeWidget::~ARCodeWidget() = default;

void ARCodeWidget::CreateWidgets()
{
  m_warning = new CheatWarningWidget(m_game_id, m_restart_required, this);
#ifdef USE_RETRO_ACHIEVEMENTS
  m_hc_warning = new HardcoreWarningWidget(this);
#endif  // USE_RETRO_ACHIEVEMENTS
  m_code_list = new QListWidget;
  m_code_add = new NonDefaultQPushButton(tr("&Add New Code..."));
  m_code_edit = new NonDefaultQPushButton(tr("&Edit Code..."));
  m_code_remove = new NonDefaultQPushButton(tr("&Remove Code"));

  m_code_list->setEnabled(!m_game_id.empty());
  m_code_add->setEnabled(!m_game_id.empty());
  m_code_edit->setEnabled(!m_game_id.empty());
  m_code_remove->setEnabled(!m_game_id.empty());

  m_code_list->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* button_layout = new QHBoxLayout;

  button_layout->addWidget(m_code_add);
  button_layout->addWidget(m_code_edit);
  button_layout->addWidget(m_code_remove);

  QVBoxLayout* layout = new QVBoxLayout;

  layout->addWidget(m_warning);
#ifdef USE_RETRO_ACHIEVEMENTS
  layout->addWidget(m_hc_warning);
#endif  // USE_RETRO_ACHIEVEMENTS
  layout->addWidget(m_code_list);
  layout->addLayout(button_layout);

  setLayout(layout);
}

void ARCodeWidget::ConnectWidgets()
{
  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this,
          &ARCodeWidget::OpenGeneralSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_hc_warning, &HardcoreWarningWidget::OpenAchievementSettings, this,
          &ARCodeWidget::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  connect(m_code_list, &QListWidget::itemChanged, this, &ARCodeWidget::OnItemChanged);
  connect(m_code_list, &QListWidget::itemSelectionChanged, this, &ARCodeWidget::OnSelectionChanged);
  connect(m_code_list->model(), &QAbstractItemModel::rowsMoved, this,
          &ARCodeWidget::OnListReordered);
  connect(m_code_list, &QListWidget::customContextMenuRequested, this,
          &ARCodeWidget::OnContextMenuRequested);

  connect(m_code_add, &QPushButton::clicked, this, &ARCodeWidget::OnCodeAddClicked);
  connect(m_code_edit, &QPushButton::clicked, this, &ARCodeWidget::OnCodeEditClicked);
  connect(m_code_remove, &QPushButton::clicked, this, &ARCodeWidget::OnCodeRemoveClicked);
}

void ARCodeWidget::OnItemChanged(QListWidgetItem* item)
{
  m_ar_codes[m_code_list->row(item)].enabled = (item->checkState() == Qt::Checked);

  if (!m_restart_required)
    ActionReplay::ApplyCodes(m_ar_codes);

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::OnContextMenuRequested()
{
  QMenu menu;

  menu.addAction(tr("Sort Alphabetically"), this, &ARCodeWidget::SortAlphabetically);
  menu.addAction(tr("Show Enabled Codes First"), this, &ARCodeWidget::SortEnabledCodesFirst);
  menu.addAction(tr("Show Disabled Codes First"), this, &ARCodeWidget::SortDisabledCodesFirst);

  menu.exec(QCursor::pos());
}

void ARCodeWidget::SortAlphabetically()
{
  m_code_list->sortItems();
  OnListReordered();
}

void ARCodeWidget::SortEnabledCodesFirst()
{
  std::stable_sort(m_ar_codes.begin(), m_ar_codes.end(), [](const auto& a, const auto& b) {
    return a.enabled && a.enabled != b.enabled;
  });

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::SortDisabledCodesFirst()
{
  std::stable_sort(m_ar_codes.begin(), m_ar_codes.end(), [](const auto& a, const auto& b) {
    return !a.enabled && a.enabled != b.enabled;
  });

  UpdateList();
  SaveCodes();
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
                                         .replace(QStringLiteral("&lt;"), QChar::fromLatin1('<'))
                                         .replace(QStringLiteral("&gt;"), QChar::fromLatin1('>')));

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                   Qt::ItemIsDragEnabled);
    item->setCheckState(ar.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, static_cast<int>(i));

    m_code_list->addItem(item);
  }

  m_code_list->setDragDropMode(QAbstractItemView::InternalMove);
}

void ARCodeWidget::SaveCodes()
{
  if (m_game_id.empty())
    return;

  const auto ini_path =
      std::string(File::GetUserPath(D_GAMESETTINGS_IDX)).append(m_game_id).append(".ini");

  Common::IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  ActionReplay::SaveCodes(&game_ini_local, m_ar_codes);
  game_ini_local.Save(ini_path);
}

void ARCodeWidget::AddCode(ActionReplay::ARCode code)
{
  m_ar_codes.push_back(std::move(code));

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::OnCodeAddClicked()
{
  ActionReplay::ARCode ar;
  ar.enabled = true;

  CheatCodeEditor ed(this);
  ed.SetARCode(&ar);
  SetQWidgetWindowDecorations(&ed);
  if (ed.exec() == QDialog::Rejected)
    return;

  m_ar_codes.push_back(std::move(ar));

  UpdateList();
  SaveCodes();
}

void ARCodeWidget::OnCodeEditClicked()
{
  const auto items = m_code_list->selectedItems();
  if (items.empty())
    return;

  const auto* const selected = items[0];
  auto& current_ar = m_ar_codes[m_code_list->row(selected)];

  CheatCodeEditor ed(this);
  if (current_ar.user_defined)
  {
    ed.SetARCode(&current_ar);

    SetQWidgetWindowDecorations(&ed);
    if (ed.exec() == QDialog::Rejected)
      return;
  }
  else
  {
    ActionReplay::ARCode ar = current_ar;
    ed.SetARCode(&ar);

    SetQWidgetWindowDecorations(&ed);
    if (ed.exec() == QDialog::Rejected)
      return;

    m_ar_codes.push_back(std::move(ar));
  }

  SaveCodes();
  UpdateList();
}

void ARCodeWidget::OnCodeRemoveClicked()
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
