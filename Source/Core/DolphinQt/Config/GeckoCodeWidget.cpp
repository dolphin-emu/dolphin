// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GeckoCodeWidget.h"

#include <algorithm>
#include <utility>

#include <QCursor>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatWarningWidget.h"
#include "DolphinQt/Config/HardcoreWarningWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "UICommon/GameFile.h"

GeckoCodeWidget::GeckoCodeWidget(std::string game_id, std::string gametdb_id, u16 game_revision,
                                 bool restart_required)
    : m_game_id(std::move(game_id)), m_gametdb_id(std::move(gametdb_id)),
      m_game_revision(game_revision), m_restart_required(restart_required)
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
    m_gecko_codes = Gecko::LoadCodes(game_ini_default, game_ini_local);
  }

  UpdateList();
}

GeckoCodeWidget::~GeckoCodeWidget() = default;

void GeckoCodeWidget::CreateWidgets()
{
  m_warning = new CheatWarningWidget(m_game_id, m_restart_required, this);
#ifdef USE_RETRO_ACHIEVEMENTS
  m_hc_warning = new HardcoreWarningWidget(this);
#endif  // USE_RETRO_ACHIEVEMENTS
  m_code_list = new QListWidget;
  m_name_label = new QLabel;
  m_creator_label = new QLabel;

  m_code_list->setContextMenuPolicy(Qt::CustomContextMenu);

  QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());

  const auto line_height = QFontMetrics(font()).lineSpacing();

  m_code_description = new QTextEdit;
  m_code_description->setFont(monospace);
  m_code_description->setReadOnly(true);
  m_code_description->setFixedHeight(line_height * 5);

  m_code_view = new QTextEdit;
  m_code_view->setFont(monospace);
  m_code_view->setReadOnly(true);
  m_code_view->setFixedHeight(line_height * 10);

  m_add_code = new NonDefaultQPushButton(tr("&Add New Code..."));
  m_edit_code = new NonDefaultQPushButton(tr("&Edit Code..."));
  m_remove_code = new NonDefaultQPushButton(tr("&Remove Code"));
  m_download_codes = new NonDefaultQPushButton(tr("Download Codes"));

  m_download_codes->setToolTip(tr("Download Codes from the WiiRD Database"));

  m_code_list->setEnabled(!m_game_id.empty());
  m_name_label->setEnabled(!m_game_id.empty());
  m_creator_label->setEnabled(!m_game_id.empty());
  m_code_description->setEnabled(!m_game_id.empty());
  m_code_view->setEnabled(!m_game_id.empty());

  m_add_code->setEnabled(!m_game_id.empty());
  m_edit_code->setEnabled(false);
  m_remove_code->setEnabled(false);
  m_download_codes->setEnabled(!m_game_id.empty());

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_warning);
#ifdef USE_RETRO_ACHIEVEMENTS
  layout->addWidget(m_hc_warning);
#endif  // USE_RETRO_ACHIEVEMENTS
  layout->addWidget(m_code_list);

  auto* info_layout = new QFormLayout;

  info_layout->addRow(tr("Name:"), m_name_label);
  info_layout->addRow(tr("Creator:"), m_creator_label);
  info_layout->addRow(tr("Description:"), static_cast<QWidget*>(nullptr));

  info_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

  for (QLabel* label : {m_name_label, m_creator_label})
  {
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setCursor(Qt::IBeamCursor);
  }

  layout->addLayout(info_layout);
  layout->addWidget(m_code_description);
  layout->addWidget(m_code_view);

  QHBoxLayout* btn_layout = new QHBoxLayout;

  btn_layout->addWidget(m_add_code);
  btn_layout->addWidget(m_edit_code);
  btn_layout->addWidget(m_remove_code);
  btn_layout->addWidget(m_download_codes);

  layout->addLayout(btn_layout);

  WrapInScrollArea(this, layout);
}

void GeckoCodeWidget::ConnectWidgets()
{
  connect(m_code_list, &QListWidget::itemSelectionChanged, this,
          &GeckoCodeWidget::OnSelectionChanged);
  connect(m_code_list, &QListWidget::itemChanged, this, &GeckoCodeWidget::OnItemChanged);
  connect(m_code_list->model(), &QAbstractItemModel::rowsMoved, this,
          &GeckoCodeWidget::OnListReordered);
  connect(m_code_list, &QListWidget::customContextMenuRequested, this,
          &GeckoCodeWidget::OnContextMenuRequested);

  connect(m_add_code, &QPushButton::clicked, this, &GeckoCodeWidget::AddCode);
  connect(m_remove_code, &QPushButton::clicked, this, &GeckoCodeWidget::RemoveCode);
  connect(m_edit_code, &QPushButton::clicked, this, &GeckoCodeWidget::EditCode);
  connect(m_download_codes, &QPushButton::clicked, this, &GeckoCodeWidget::DownloadCodes);
  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this,
          &GeckoCodeWidget::OpenGeneralSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(m_hc_warning, &HardcoreWarningWidget::OpenAchievementSettings, this,
          &GeckoCodeWidget::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS
}

void GeckoCodeWidget::OnSelectionChanged()
{
  auto items = m_code_list->selectedItems();

  const bool empty = items.empty();

  m_edit_code->setEnabled(!empty);
  m_remove_code->setEnabled(!empty);

  if (items.empty())
    return;

  auto selected = items[0];

  const int index = selected->data(Qt::UserRole).toInt();

  const auto& code = m_gecko_codes[index];

  m_name_label->setText(QString::fromStdString(code.name));
  m_creator_label->setText(QString::fromStdString(code.creator));

  m_code_description->clear();

  for (const auto& line : code.notes)
    m_code_description->append(QString::fromStdString(line));

  m_code_view->clear();

  for (const auto& c : code.codes)
    m_code_view->append(QString::fromStdString(c.original_line));
}

void GeckoCodeWidget::OnItemChanged(QListWidgetItem* item)
{
  const int index = item->data(Qt::UserRole).toInt();
  m_gecko_codes[index].enabled = (item->checkState() == Qt::Checked);

  if (!m_restart_required)
    Gecko::SetActiveCodes(m_gecko_codes);

  SaveCodes();
}

void GeckoCodeWidget::AddCode()
{
  Gecko::GeckoCode code;
  code.enabled = true;

  CheatCodeEditor ed(this);
  ed.SetGeckoCode(&code);
  SetQWidgetWindowDecorations(&ed);
  if (ed.exec() == QDialog::Rejected)
    return;

  m_gecko_codes.push_back(std::move(code));
  SaveCodes();
  UpdateList();
}

void GeckoCodeWidget::EditCode()
{
  const auto* item = m_code_list->currentItem();
  if (item == nullptr)
    return;

  const int index = item->data(Qt::UserRole).toInt();

  CheatCodeEditor ed(this);
  ed.SetGeckoCode(&m_gecko_codes[index]);
  SetQWidgetWindowDecorations(&ed);
  if (ed.exec() == QDialog::Rejected)
    return;

  SaveCodes();
  UpdateList();
}

void GeckoCodeWidget::RemoveCode()
{
  const auto* item = m_code_list->currentItem();

  if (item == nullptr)
    return;

  m_gecko_codes.erase(m_gecko_codes.begin() + item->data(Qt::UserRole).toInt());

  UpdateList();
  SaveCodes();
}

void GeckoCodeWidget::SaveCodes()
{
  if (m_game_id.empty())
    return;

  const auto ini_path =
      std::string(File::GetUserPath(D_GAMESETTINGS_IDX)).append(m_game_id).append(".ini");

  Common::IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  Gecko::SaveCodes(game_ini_local, m_gecko_codes);
  game_ini_local.Save(ini_path);
}

void GeckoCodeWidget::OnContextMenuRequested()
{
  QMenu menu;

  menu.addAction(tr("Sort Alphabetically"), this, &GeckoCodeWidget::SortAlphabetically);
  menu.addAction(tr("Show Enabled Codes First"), this, &GeckoCodeWidget::SortEnabledCodesFirst);
  menu.addAction(tr("Show Disabled Codes First"), this, &GeckoCodeWidget::SortDisabledCodesFirst);

  menu.exec(QCursor::pos());
}

void GeckoCodeWidget::SortAlphabetically()
{
  m_code_list->sortItems();
  OnListReordered();
}

void GeckoCodeWidget::SortEnabledCodesFirst()
{
  std::ranges::stable_sort(m_gecko_codes, [](const auto& a, const auto& b) {
    return a.enabled && a.enabled != b.enabled;
  });

  UpdateList();
  SaveCodes();
}

void GeckoCodeWidget::SortDisabledCodesFirst()
{
  std::ranges::stable_sort(m_gecko_codes, [](const auto& a, const auto& b) {
    return !a.enabled && a.enabled != b.enabled;
  });

  UpdateList();
  SaveCodes();
}

void GeckoCodeWidget::OnListReordered()
{
  // Reorder codes based on the indices of table item
  std::vector<Gecko::GeckoCode> codes;
  codes.reserve(m_gecko_codes.size());

  for (int i = 0; i < m_code_list->count(); i++)
  {
    const int index = m_code_list->item(i)->data(Qt::UserRole).toInt();

    codes.push_back(std::move(m_gecko_codes[index]));
  }

  m_gecko_codes = std::move(codes);

  UpdateList();
  SaveCodes();
}

void GeckoCodeWidget::UpdateList()
{
  m_code_list->clear();

  for (size_t i = 0; i < m_gecko_codes.size(); i++)
  {
    const auto& code = m_gecko_codes[i];

    auto* item = new QListWidgetItem(QString::fromStdString(code.name)
                                         .replace(QStringLiteral("&lt;"), QChar::fromLatin1('<'))
                                         .replace(QStringLiteral("&gt;"), QChar::fromLatin1('>')));

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                   Qt::ItemIsDragEnabled);
    item->setCheckState(code.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, static_cast<int>(i));

    m_code_list->addItem(item);
  }

  m_code_list->setDragDropMode(QAbstractItemView::InternalMove);
}

void GeckoCodeWidget::DownloadCodes()
{
  bool success;

  std::vector<Gecko::GeckoCode> codes = Gecko::DownloadCodes(m_gametdb_id, &success);

  if (!success)
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to download codes."));
    return;
  }

  if (codes.empty())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("File contained no codes."));
    return;
  }

  size_t added_count = 0;

  for (const auto& code : codes)
  {
    auto it = std::ranges::find(m_gecko_codes, code);

    if (it == m_gecko_codes.end())
    {
      m_gecko_codes.push_back(code);
      added_count++;
    }
  }

  UpdateList();
  SaveCodes();

  ModalMessageBox::information(
      this, tr("Download complete"),
      tr("Downloaded %1 codes. (added %2)")
          .arg(QString::number(codes.size()), QString::number(added_count)));
}
