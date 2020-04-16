// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/CheatList.h"

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

#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PatchEngine.h"

#include "DolphinQt/Config/CheatCodeEditor.h"
#include "DolphinQt/Config/CheatItem.h"
#include "DolphinQt/Config/CheatWarningWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "UICommon/GameFile.h"


CheatList::CheatList(const UICommon::GameFile& game, CheatType type, bool restart_required)
    : m_game(game), m_game_id(game.GetGameID()), m_gametdb_id(game.GetGameTDBID()),
      m_game_revision(game.GetRevision()), m_restart_required(restart_required)
{
  m_cheat_type = type;

  CreateWidgets();
  ConnectWidgets();

  IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini");

  const IniFile game_ini_default = SConfig::LoadDefaultGameIni(m_game_id, m_game_revision);

  switch (m_cheat_type)
  {
  case CheatType::GeckoCode:
    {
      auto gecko_codes = Gecko::LoadCodes(game_ini_default, game_ini_local);
      for (auto &code : gecko_codes) {
        m_code_list->addItem(new CheatItem(code));
      }
    }
    break;
  case CheatType::ARCode:
    {
      auto ar_codes = ActionReplay::LoadCodes(game_ini_default, game_ini_local);
      for (auto &code : ar_codes) {
        m_code_list->addItem(new CheatItem(code));
      }
    }
    break;
  case CheatType::DolphinPatch:
    {
      std::vector<PatchEngine::Patch> frame_patches;
      PatchEngine::LoadPatchSection("OnFrame", frame_patches, game_ini_default, game_ini_local);

      for (auto &patch : frame_patches) {
        m_code_list->addItem(new CheatItem(patch));
      }
    }
    break;
  }
}

CheatList::~CheatList() = default;

void CheatList::CreateWidgets()
{
  // Dolphin's patches aren't considered cheats and can be applid even when cheats are disabled
  bool is_cheat = m_cheat_type != CheatType::DolphinPatch;

  m_warning = new CheatWarningWidget(m_game_id, is_cheat, m_restart_required, this);
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
  m_code_list->setDragDropMode(QAbstractItemView::InternalMove);

  m_add_code = new QPushButton(tr("&Add New Code..."));
  m_edit_code = new QPushButton(tr("&Edit Code..."));
  m_remove_code = new QPushButton(tr("&Remove Code"));

  m_download_codes = new QPushButton(tr("Download Codes"));

  m_download_codes->setToolTip(tr("Download Codes from the WiiRD Database"));

  m_download_codes->setEnabled(!m_game_id.empty());

  m_edit_code->setEnabled(false);
  m_remove_code->setEnabled(false);

  auto* layout = new QVBoxLayout;

  layout->addWidget(m_warning);
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

  if (m_cheat_type == CheatType::GeckoCode) { // Only Gecko codes can be downloaded
    btn_layout->addWidget(m_download_codes);
  }

  layout->addLayout(btn_layout);

  setLayout(layout);
}

void CheatList::ConnectWidgets()
{
  connect(m_code_list, &QListWidget::itemSelectionChanged, this,
          &CheatList::OnSelectionChanged);
  connect(m_code_list, &QListWidget::itemChanged, this, &CheatList::OnItemChanged);
  connect(m_code_list->model(), &QAbstractItemModel::rowsMoved, this,
          &CheatList::SaveCodes);
  connect(m_code_list, &QListWidget::customContextMenuRequested, this,
          &CheatList::OnContextMenuRequested);

  connect(m_add_code, &QPushButton::clicked, this, &CheatList::AddCodeClicked);
  connect(m_remove_code, &QPushButton::clicked, this, &CheatList::RemoveCode);
  connect(m_edit_code, &QPushButton::clicked, this, &CheatList::EditCode);
  connect(m_download_codes, &QPushButton::clicked, this, &CheatList::DownloadCodes);
  connect(m_warning, &CheatWarningWidget::OpenCheatEnableSettings, this,
          &CheatList::OpenGeneralSettings);
}

void CheatList::OnSelectionChanged()
{
  auto items = m_code_list->selectedItems();

  const bool empty = items.empty();

  m_edit_code->setEnabled(!empty);
  m_remove_code->setEnabled(!empty);

  if (items.empty())
    return;

  auto code = dynamic_cast<CheatItem*>(items[0]);

  m_name_label->setText(code->GetName());
  m_creator_label->setText(code->GetCreator());
  m_code_description->setText(code->GetNotes());
  m_code_view->setText(code->GetCode());
}

void CheatList::OnItemChanged(QListWidgetItem* item)
{
  if (!m_restart_required) {
    switch (m_cheat_type)
    {
    case CheatType::GeckoCode:
      {
        auto codes = GetList<Gecko::GeckoCode>();
        Gecko::SetActiveCodes(codes);
      }
      break;
    case CheatType::ARCode:
      {
        auto codes = GetList<ActionReplay::ARCode>();
        ActionReplay::ApplyCodes(codes);
      }
      break;
    case CheatType::DolphinPatch:
      // Dolphin patches can't be updated at runtime
      break;
    }
  }

  SaveCodes();
}

template<typename T>
void CheatList::AddCode(T code) {
  m_code_list->addItem(new CheatItem(code));

  SaveCodes();
}

template void CheatList::AddCode(ActionReplay::ARCode);
template void CheatList::AddCode(Gecko::GeckoCode);
template void CheatList::AddCode(PatchEngine::Patch);

void CheatList::AddCodeClicked()
{
  CheatItem *new_list_item;

  CheatCodeEditor ed(this);

  switch (m_cheat_type)
  {
  case CheatType::GeckoCode:
    {
      Gecko::GeckoCode new_code;
      ed.SetGeckoCode(&new_code);

      if (ed.exec() == QDialog::Rejected)
        return;

      new_list_item = new CheatItem(new_code);
    }
    break;
  case CheatType::ARCode:
    {
      ActionReplay::ARCode new_code;
      ed.SetARCode(&new_code);

      if (ed.exec() == QDialog::Rejected)
        return;

      new_list_item = new CheatItem(new_code);
    }
    break;
  case CheatType::DolphinPatch:
    {
      PatchEngine::Patch new_code;
      ed.SetDolphinPatch(&new_code);

      if (ed.exec() == QDialog::Rejected)
        return;

      new_list_item = new CheatItem(new_code);
    }
    break;
  }

  new_list_item->setSelected(true);
  m_code_list->addItem(new_list_item);

  SaveCodes();
}

void CheatList::EditCode()
{
  auto* item = m_code_list->currentItem();
  if (item == nullptr)
    return;

  auto code = dynamic_cast<CheatItem*>(item);

  CheatCodeEditor ed(this);

  switch (m_cheat_type)
  {
  case CheatType::GeckoCode:
    {
      auto gecko_code = code->GetRawCode<Gecko::GeckoCode>();
      ed.SetGeckoCode(gecko_code);
    }
    break;
  case CheatType::ARCode:
    {
      auto ar_code = code->GetRawCode<ActionReplay::ARCode>();
      ed.SetARCode(ar_code);
    }
    break;
  case CheatType::DolphinPatch:
    auto patch = code->GetRawCode<PatchEngine::Patch>();
    ed.SetDolphinPatch(patch);
    break;
  }

  if (ed.exec() == QDialog::Rejected)
    return;

  SaveCodes();
}

void CheatList::RemoveCode()
{
  auto* item = m_code_list->currentItem();

  if (item == nullptr)
    return;

  delete item;

  SaveCodes();
}

void CheatList::SaveCodes()
{
  const auto ini_path =
      std::string(File::GetUserPath(D_GAMESETTINGS_IDX)).append(m_game_id).append(".ini");

  IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  switch (m_cheat_type)
    {
    case CheatType::GeckoCode:
      {
        auto codes = GetList<Gecko::GeckoCode>();
        Gecko::SaveCodes(game_ini_local, codes);
      }
      break;
    case CheatType::ARCode:
      {
        auto codes = GetList<ActionReplay::ARCode>();
        ActionReplay::SaveCodes(game_ini_local, codes);
      }
      break;
    case CheatType::DolphinPatch:
      {
        auto patches = GetList<PatchEngine::Patch>();
        PatchEngine::SavePatches(game_ini_local, patches);
      }
      break;
    }

  game_ini_local.Save(ini_path);
}

void CheatList::OnContextMenuRequested()
{
  QMenu menu;

  menu.addAction(tr("Sort Alphabetically"), this, &CheatList::SortAlphabetically);

  menu.exec(QCursor::pos());
}

void CheatList::SortAlphabetically()
{
  m_code_list->sortItems();
  SaveCodes();
}

void CheatList::DownloadCodes()
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

  auto current_codes = GetList<Gecko::GeckoCode>();

  for (const auto& code : codes)
  {
    auto it = std::find(current_codes.begin(), current_codes.end(), code);

    if (it == current_codes.end())
    {
      m_code_list->addItem(new CheatItem(code));
      added_count++;
    }
  }

  SaveCodes();

  ModalMessageBox::information(
      this, tr("Download complete"),
      tr("Downloaded %1 codes. (added %2)")
          .arg(QString::number(codes.size()), QString::number(added_count)));
}

template<typename T>
std::vector<T> CheatList::GetList() {
  int count = m_code_list->count();
  std::vector<T> list;
  list.reserve(count);

  for (int i = 0; i < count; i++) {
    auto code = dynamic_cast<CheatItem*>(m_code_list->item(i));
    list.push_back(*code->GetRawCode<T>());
  }

  return list;
}