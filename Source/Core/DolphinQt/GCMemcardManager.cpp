// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GCMemcardManager.h"

#include <algorithm>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCMemcard/GCMemcardUtils.h"

#include "DolphinQt/GCMemcardCreateNewDialog.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

using namespace ExpansionInterface;

constexpr int ROW_HEIGHT = 36;
constexpr int COLUMN_WIDTH_FILENAME = 100;
constexpr int COLUMN_WIDTH_BANNER = Memcard::MEMORY_CARD_BANNER_WIDTH + 6;
constexpr int COLUMN_WIDTH_TEXT = 160;
constexpr int COLUMN_WIDTH_ICON = Memcard::MEMORY_CARD_ICON_WIDTH + 6;
constexpr int COLUMN_WIDTH_BLOCKS = 40;
constexpr int COLUMN_INDEX_FILENAME = 0;
constexpr int COLUMN_INDEX_BANNER = 1;
constexpr int COLUMN_INDEX_TEXT = 2;
constexpr int COLUMN_INDEX_ICON = 3;
constexpr int COLUMN_INDEX_BLOCKS = 4;
constexpr int COLUMN_COUNT = 5;

namespace
{
Slot OtherSlot(Slot slot)
{
  return slot == Slot::A ? Slot::B : Slot::A;
}
}  // namespace

struct GCMemcardManager::IconAnimationData
{
  // the individual frames
  std::vector<QPixmap> m_frames;

  // vector containing a list of frame indices that indicate, for each time unit,
  // the frame that should be displayed when at that time unit
  std::vector<u8> m_frame_timing;
};

GCMemcardManager::GCMemcardManager(QWidget* parent) : QDialog(parent)
{
  CreateWidgets();
  ConnectWidgets();

  SetActiveSlot(Slot::A);
  UpdateActions();

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &GCMemcardManager::DrawIcons);

  // individual frames of icon animations can stay on screen for 4, 8, or 12 frames at 60 FPS,
  // which means the fastest animation and common denominator is 15 FPS or 66 milliseconds per frame
  m_timer->start(1000 / 15);

  LoadDefaultMemcards();

  // Make the dimensions more reasonable on startup
  resize(650, 500);

  setWindowTitle(tr("GameCube Memory Card Manager"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

GCMemcardManager::~GCMemcardManager() = default;

void GCMemcardManager::CreateWidgets()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  // Actions
  m_select_button = new NonDefaultQPushButton;
  m_copy_button = new NonDefaultQPushButton;
  m_delete_button = new NonDefaultQPushButton(tr("&Delete"));

  m_export_button = new QToolButton(this);
  m_export_menu = new QMenu(m_export_button);
  m_export_gci_action = new QAction(tr("&Export as .gci..."), m_export_menu);
  m_export_gcs_action = new QAction(tr("Export as .&gcs..."), m_export_menu);
  m_export_sav_action = new QAction(tr("Export as .&sav..."), m_export_menu);
  m_export_menu->addAction(m_export_gci_action);
  m_export_menu->addAction(m_export_gcs_action);
  m_export_menu->addAction(m_export_sav_action);
  m_export_button->setDefaultAction(m_export_gci_action);
  m_export_button->setPopupMode(QToolButton::MenuButtonPopup);
  m_export_button->setMenu(m_export_menu);

  m_import_button = new NonDefaultQPushButton(tr("&Import..."));
  m_fix_checksums_button = new NonDefaultQPushButton(tr("Fix Checksums"));

  auto* layout = new QGridLayout;

  for (Slot slot : MEMCARD_SLOTS)
  {
    m_slot_group[slot] = new QGroupBox(slot == Slot::A ? tr("Slot A") : tr("Slot B"));
    m_slot_file_edit[slot] = new QLineEdit;
    m_slot_open_button[slot] = new NonDefaultQPushButton(tr("&Open..."));
    m_slot_create_button[slot] = new NonDefaultQPushButton(tr("&Create..."));
    m_slot_table[slot] = new QTableWidget;
    m_slot_table[slot]->setTabKeyNavigation(false);
    m_slot_stat_label[slot] = new QLabel;

    m_slot_table[slot]->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_slot_table[slot]->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_slot_table[slot]->setSortingEnabled(true);
    m_slot_table[slot]->horizontalHeader()->setHighlightSections(false);
    m_slot_table[slot]->horizontalHeader()->setMinimumSectionSize(0);
    m_slot_table[slot]->horizontalHeader()->setSortIndicatorShown(true);
    m_slot_table[slot]->setColumnCount(COLUMN_COUNT);
    m_slot_table[slot]->setHorizontalHeaderItem(COLUMN_INDEX_FILENAME,
                                                new QTableWidgetItem(tr("Filename")));
    m_slot_table[slot]->setHorizontalHeaderItem(COLUMN_INDEX_BANNER,
                                                new QTableWidgetItem(tr("Banner")));
    m_slot_table[slot]->setHorizontalHeaderItem(COLUMN_INDEX_TEXT,
                                                new QTableWidgetItem(tr("Title")));
    m_slot_table[slot]->setHorizontalHeaderItem(COLUMN_INDEX_ICON,
                                                new QTableWidgetItem(tr("Icon")));
    m_slot_table[slot]->setHorizontalHeaderItem(COLUMN_INDEX_BLOCKS,
                                                new QTableWidgetItem(tr("Blocks")));
    m_slot_table[slot]->setColumnWidth(COLUMN_INDEX_FILENAME, COLUMN_WIDTH_FILENAME);
    m_slot_table[slot]->setColumnWidth(COLUMN_INDEX_BANNER, COLUMN_WIDTH_BANNER);
    m_slot_table[slot]->setColumnWidth(COLUMN_INDEX_TEXT, COLUMN_WIDTH_TEXT);
    m_slot_table[slot]->setColumnWidth(COLUMN_INDEX_ICON, COLUMN_WIDTH_ICON);
    m_slot_table[slot]->setColumnWidth(COLUMN_INDEX_BLOCKS, COLUMN_WIDTH_BLOCKS);
    m_slot_table[slot]->verticalHeader()->setDefaultSectionSize(ROW_HEIGHT);
    m_slot_table[slot]->verticalHeader()->hide();
    m_slot_table[slot]->setShowGrid(false);

    auto* slot_layout = new QGridLayout;
    m_slot_group[slot]->setLayout(slot_layout);

    slot_layout->addWidget(m_slot_file_edit[slot], 0, 0);
    slot_layout->addWidget(m_slot_open_button[slot], 0, 1);
    slot_layout->addWidget(m_slot_create_button[slot], 0, 2);
    slot_layout->addWidget(m_slot_table[slot], 1, 0, 1, 3);
    slot_layout->addWidget(m_slot_stat_label[slot], 2, 0);

    layout->addWidget(m_slot_group[slot], 0, slot == Slot::A ? 0 : 2, 8, 1);

    UpdateSlotTable(slot);
  }

  layout->addWidget(m_select_button, 1, 1);
  layout->addWidget(m_copy_button, 2, 1);
  layout->addWidget(m_delete_button, 3, 1);
  layout->addWidget(m_export_button, 4, 1);
  layout->addWidget(m_import_button, 5, 1);
  layout->addWidget(m_fix_checksums_button, 6, 1);
  layout->addWidget(m_button_box, 8, 2);

  setLayout(layout);
}

void GCMemcardManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_select_button, &QPushButton::clicked,
          [this] { SetActiveSlot(OtherSlot(m_active_slot)); });
  connect(m_export_gci_action, &QAction::triggered,
          [this] { ExportFiles(Memcard::SavefileFormat::GCI); });
  connect(m_export_gcs_action, &QAction::triggered,
          [this] { ExportFiles(Memcard::SavefileFormat::GCS); });
  connect(m_export_sav_action, &QAction::triggered,
          [this] { ExportFiles(Memcard::SavefileFormat::SAV); });
  connect(m_delete_button, &QPushButton::clicked, this, &GCMemcardManager::DeleteFiles);
  connect(m_import_button, &QPushButton::clicked, this, &GCMemcardManager::ImportFile);
  connect(m_copy_button, &QPushButton::clicked, this, &GCMemcardManager::CopyFiles);
  connect(m_fix_checksums_button, &QPushButton::clicked, this, &GCMemcardManager::FixChecksums);

  for (Slot slot : MEMCARD_SLOTS)
  {
    connect(m_slot_file_edit[slot], &QLineEdit::textChanged,
            [this, slot](const QString& path) { SetSlotFile(slot, path); });
    connect(m_slot_open_button[slot], &QPushButton::clicked,
            [this, slot] { SetSlotFileInteractive(slot); });
    connect(m_slot_create_button[slot], &QPushButton::clicked,
            [this, slot] { CreateNewCard(slot); });
    connect(m_slot_table[slot], &QTableWidget::itemSelectionChanged, this,
            &GCMemcardManager::UpdateActions);
  }
}

void GCMemcardManager::LoadDefaultMemcards()
{
  for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
  {
    if (Config::Get(Config::GetInfoForEXIDevice(slot)) !=
        ExpansionInterface::EXIDeviceType::MemoryCard)
    {
      continue;
    }

    const QString path = QString::fromStdString(
        Config::GetMemcardPath(slot, Config::Get(Config::MAIN_FALLBACK_REGION)));
    SetSlotFile(slot, path);
  }
}

void GCMemcardManager::SetActiveSlot(Slot slot)
{
  for (Slot slot2 : MEMCARD_SLOTS)
    m_slot_table[slot2]->setEnabled(slot2 == slot);

  m_select_button->setText(slot == Slot::A ? tr("Switch to B") : tr("Switch to A"));
  m_copy_button->setText(slot == Slot::A ? tr("Copy to B") : tr("Copy to A"));

  m_active_slot = slot;

  UpdateSlotTable(slot);
  UpdateActions();
}

void GCMemcardManager::UpdateSlotTable(Slot slot)
{
  m_slot_active_icons[slot].clear();

  if (m_slot_memcard[slot] == nullptr)
  {
    m_slot_table[slot]->setRowCount(0);
    m_slot_stat_label[slot]->clear();
    return;
  }

  auto& memcard = m_slot_memcard[slot];
  auto* table = m_slot_table[slot];
  table->setSortingEnabled(false);

  const u8 num_files = memcard->GetNumFiles();
  const u8 free_files = Memcard::DIRLEN - num_files;
  const u16 free_blocks = memcard->GetFreeBlocks();
  table->setRowCount(num_files);
  for (int i = 0; i < num_files; i++)
  {
    const u8 file_index = memcard->GetFileIndex(i);

    const auto file_comments = memcard->GetSaveComments(file_index);
    const u16 block_count = memcard->DEntry_BlockCount(file_index);
    const auto entry = memcard->GetDEntry(file_index);
    const std::string filename = entry ? Memcard::GenerateFilename(*entry) : "";

    const QString title =
        file_comments ? QString::fromStdString(file_comments->first).trimmed() : QString();
    const QString comment =
        file_comments ? QString::fromStdString(file_comments->second).trimmed() : QString();
    auto banner = GetBannerFromSaveFile(file_index, slot);
    auto icon_data = GetIconFromSaveFile(file_index, slot);

    auto* item_filename = new QTableWidgetItem(QString::fromStdString(filename));
    auto* item_banner = new QTableWidgetItem();
    auto* item_text = new QTableWidgetItem(QStringLiteral("%1\n%2").arg(title, comment));
    auto* item_icon = new QTableWidgetItem();
    auto* item_blocks = new QTableWidgetItem();

    item_banner->setData(Qt::DecorationRole, banner);
    item_icon->setData(Qt::DecorationRole, icon_data.m_frames[0]);
    item_blocks->setData(Qt::DisplayRole, block_count);

    for (auto* item : {item_filename, item_banner, item_text, item_icon, item_blocks})
    {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setData(Qt::UserRole, static_cast<int>(file_index));
    }

    m_slot_active_icons[slot].emplace(file_index, std::move(icon_data));

    table->setItem(i, COLUMN_INDEX_FILENAME, item_filename);
    table->setItem(i, COLUMN_INDEX_BANNER, item_banner);
    table->setItem(i, COLUMN_INDEX_TEXT, item_text);
    table->setItem(i, COLUMN_INDEX_ICON, item_icon);
    table->setItem(i, COLUMN_INDEX_BLOCKS, item_blocks);
  }

  const QString free_blocks_string = tr("Free Blocks: %1").arg(free_blocks);
  const QString free_files_string = tr("Free Files: %1").arg(free_files);
  m_slot_stat_label[slot]->setText(
      QStringLiteral("%1      %2").arg(free_blocks_string, free_files_string));

  table->setSortingEnabled(true);
}

void GCMemcardManager::UpdateActions()
{
  auto selection = m_slot_table[m_active_slot]->selectedItems();
  bool have_selection = selection.count();
  bool have_memcard = m_slot_memcard[m_active_slot] != nullptr;
  bool have_memcard_other = m_slot_memcard[OtherSlot(m_active_slot)] != nullptr;

  m_copy_button->setEnabled(have_selection && have_memcard_other);
  m_export_button->setEnabled(have_selection);
  m_import_button->setEnabled(have_memcard);
  m_delete_button->setEnabled(have_selection);
  m_fix_checksums_button->setEnabled(have_memcard);
}

void GCMemcardManager::SetSlotFile(Slot slot, QString path)
{
  auto [error_code, memcard] = Memcard::GCMemcard::Open(path.toStdString());

  if (!error_code.HasCriticalErrors() && memcard && memcard->IsValid())
  {
    m_slot_file_edit[slot]->setText(path);
    m_slot_memcard[slot] = std::make_unique<Memcard::GCMemcard>(std::move(*memcard));
  }
  else
  {
    m_slot_memcard[slot] = nullptr;
    ModalMessageBox::warning(
        this, tr("Error"),
        tr("Failed opening memory card:\n%1").arg(GetErrorMessagesForErrorCode(error_code)));
  }

  UpdateSlotTable(slot);
  UpdateActions();
}

void GCMemcardManager::SetSlotFileInteractive(Slot slot)
{
  QString path = QDir::toNativeSeparators(
      DolphinFileDialog::getOpenFileName(this,
                                         slot == Slot::A ? tr("Set Memory Card File for Slot A") :
                                                           tr("Set Memory Card File for Slot B"),
                                         QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
                                         QStringLiteral("%1 (*.raw *.gcp);;%2 (*)")
                                             .arg(tr("GameCube Memory Cards"), tr("All Files"))));
  if (!path.isEmpty())
    m_slot_file_edit[slot]->setText(path);
}

std::vector<u8> GCMemcardManager::GetSelectedFileIndices()
{
  const auto selection = m_slot_table[m_active_slot]->selectedItems();
  std::vector<bool> lookup(Memcard::DIRLEN);
  for (const auto* item : selection)
  {
    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= static_cast<int>(Memcard::DIRLEN))
    {
      ModalMessageBox::warning(this, tr("Error"),
                               tr("Data inconsistency in GCMemcardManager, aborting action."));
      return {};
    }
    lookup[index] = true;
  }

  std::vector<u8> selected_indices;
  for (u8 i = 0; i < Memcard::DIRLEN; ++i)
  {
    if (lookup[i])
      selected_indices.push_back(i);
  }

  return selected_indices;
}

static QString GetFormatDescription(Memcard::SavefileFormat format)
{
  switch (format)
  {
  case Memcard::SavefileFormat::GCI:
    return QObject::tr("Native GCI File");
  case Memcard::SavefileFormat::GCS:
    return QObject::tr("MadCatz Gameshark files");
  case Memcard::SavefileFormat::SAV:
    return QObject::tr("Datel MaxDrive/Pro files");
  default:
    ASSERT(false);
    return QObject::tr("Native GCI File");
  }
}

void GCMemcardManager::ExportFiles(Memcard::SavefileFormat format)
{
  const auto& memcard = m_slot_memcard[m_active_slot];
  if (!memcard)
    return;

  const auto selected_indices = GetSelectedFileIndices();
  if (selected_indices.empty())
    return;

  const auto savefiles = Memcard::GetSavefiles(*memcard, selected_indices);
  if (savefiles.empty())
  {
    ModalMessageBox::warning(this, tr("Export Failed"),
                             tr("Failed to read selected savefile(s) from memory card."));
    return;
  }

  std::string extension = Memcard::GetDefaultExtension(format);

  if (savefiles.size() == 1)
  {
    // when exporting a single save file, let user specify exact path
    const std::string basename = Memcard::GenerateFilename(savefiles[0].dir_entry);
    const QString qformatdesc = GetFormatDescription(format);
    const std::string default_path =
        fmt::format("{}/{}{}", File::GetUserPath(D_GCUSER_IDX), basename, extension);
    const QString qfilename = DolphinFileDialog::getSaveFileName(
        this, tr("Export Save File"), QString::fromStdString(default_path),
        QStringLiteral("%1 (*%2);;%3 (*)")
            .arg(qformatdesc, QString::fromStdString(extension), tr("All Files")));
    if (qfilename.isEmpty())
      return;

    const std::string filename = qfilename.toStdString();
    if (!Memcard::WriteSavefile(filename, savefiles[0], format))
    {
      File::Delete(filename);
      ModalMessageBox::warning(this, tr("Export Failed"), tr("Failed to write savefile to disk."));
    }

    return;
  }

  const QString qdirpath = DolphinFileDialog::getExistingDirectory(
      this, QObject::tr("Export Save Files"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)));
  if (qdirpath.isEmpty())
    return;

  const std::string dirpath = qdirpath.toStdString();
  size_t failures = 0;
  for (const auto& savefile : savefiles)
  {
    // find a free filename so we don't overwrite anything
    const std::string basepath = dirpath + DIR_SEP + Memcard::GenerateFilename(savefile.dir_entry);
    std::string filename = basepath + extension;
    if (File::Exists(filename))
    {
      size_t tmp = 0;
      std::string free_name;
      do
      {
        free_name = fmt::format("{}_{}{}", basepath, tmp, extension);
        ++tmp;
      } while (File::Exists(free_name));
      filename = free_name;
    }

    if (!Memcard::WriteSavefile(filename, savefile, format))
    {
      File::Delete(filename);
      ++failures;
    }
  }

  if (failures > 0)
  {
    QString failure_string =
        tr("Failed to export %n out of %1 save file(s).", "", static_cast<int>(failures))
            .arg(savefiles.size());
    if (failures == savefiles.size())
    {
      ModalMessageBox::warning(this, tr("Export Failed"), failure_string);
    }
    else
    {
      QString success_string = tr("Successfully exported %n out of %1 save file(s).", "",
                                  static_cast<int>(savefiles.size() - failures))
                                   .arg(savefiles.size());
      ModalMessageBox::warning(this, tr("Export Failed"),
                               QStringLiteral("%1\n%2").arg(failure_string, success_string));
    }
  }
}

void GCMemcardManager::ImportFiles(Slot slot, std::span<const Memcard::Savefile> savefiles)
{
  auto& card = m_slot_memcard[slot];
  if (!card)
    return;

  const size_t number_of_files = savefiles.size();
  const size_t number_of_blocks = Memcard::GetBlockCount(savefiles);
  const size_t free_files = Memcard::DIRLEN - card->GetNumFiles();
  const size_t free_blocks = card->GetFreeBlocks();

  QStringList error_messages;

  if (number_of_files > free_files)
  {
    error_messages.push_back(
        tr("Not enough free files on the target memory card. At least %n free file(s) required.",
           "", static_cast<int>(number_of_files)));
  }

  if (number_of_blocks > free_blocks)
  {
    error_messages.push_back(
        tr("Not enough free blocks on the target memory card. At least %n free block(s) required.",
           "", static_cast<int>(number_of_blocks)));
  }

  if (Memcard::HasDuplicateIdentity(savefiles))
  {
    error_messages.push_back(
        tr("At least two of the selected save files have the same internal filename."));
  }

  for (const Memcard::Savefile& savefile : savefiles)
  {
    if (card->TitlePresent(savefile.dir_entry))
    {
      const std::string filename = Memcard::GenerateFilename(savefile.dir_entry);
      error_messages.push_back(tr("The target memory card already contains a file \"%1\".")
                                   .arg(QString::fromStdString(filename)));
    }
  }

  if (!error_messages.empty())
  {
    ModalMessageBox::warning(this, tr("Import Failed"), error_messages.join(QLatin1Char('\n')));
    return;
  }

  for (const Memcard::Savefile& savefile : savefiles)
  {
    const auto result = card->ImportFile(savefile);

    // we've already checked everything that could realistically fail here, so this should only
    // happen if the memory card data is corrupted in some way
    if (result != Memcard::GCMemcardImportFileRetVal::SUCCESS)
    {
      const std::string filename = Memcard::GenerateFilename(savefile.dir_entry);
      ModalMessageBox::warning(
          this, tr("Import Failed"),
          tr("Failed to import \"%1\".").arg(QString::fromStdString(filename)));
      break;
    }
  }

  if (!card->Save())
  {
    ModalMessageBox::warning(this, tr("Import Failed"),
                             tr("Failed to write modified memory card to disk."));
  }

  UpdateSlotTable(slot);
}

void GCMemcardManager::ImportFile()
{
  auto& card = m_slot_memcard[m_active_slot];
  if (!card)
    return;

  const QStringList paths = DolphinFileDialog::getOpenFileNames(
      this, tr("Import Save File(s)"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      QStringLiteral("%1 (*.gci *.gcs *.sav);;%2 (*.gci);;%3 (*.gcs);;%4 (*.sav);;%5 (*)")
          .arg(tr("Supported file formats"), GetFormatDescription(Memcard::SavefileFormat::GCI),
               GetFormatDescription(Memcard::SavefileFormat::GCS),
               GetFormatDescription(Memcard::SavefileFormat::SAV), tr("All Files")));

  if (paths.isEmpty())
    return;

  std::vector<Memcard::Savefile> savefiles;
  savefiles.reserve(paths.size());
  QStringList errors;
  for (const QString& path : paths)
  {
    auto read_result = Memcard::ReadSavefile(path.toStdString());
    std::visit(overloaded{
                   [&](Memcard::Savefile savefile) { savefiles.emplace_back(std::move(savefile)); },
                   [&](Memcard::ReadSavefileErrorCode error_code) {
                     errors.push_back(
                         tr("%1: %2").arg(path, GetErrorMessageForErrorCode(error_code)));
                   },
               },
               std::move(read_result));
  }

  if (!errors.empty())
  {
    ModalMessageBox::warning(
        this, tr("Import Failed"),
        tr("Encountered the following errors while opening save files:\n%1\n\nAborting import.")
            .arg(errors.join(QStringLiteral("\n"))));
    return;
  }

  ImportFiles(m_active_slot, savefiles);
}

void GCMemcardManager::CopyFiles()
{
  const auto& source_card = m_slot_memcard[m_active_slot];
  if (!source_card)
    return;

  auto& target_card = m_slot_memcard[OtherSlot(m_active_slot)];
  if (!target_card)
    return;

  const auto selected_indices = GetSelectedFileIndices();
  if (selected_indices.empty())
    return;

  const auto savefiles = Memcard::GetSavefiles(*source_card, selected_indices);
  if (savefiles.empty())
  {
    ModalMessageBox::warning(this, tr("Copy Failed"),
                             tr("Failed to read selected savefile(s) from memory card."));
    return;
  }

  ImportFiles(OtherSlot(m_active_slot), savefiles);
}

void GCMemcardManager::DeleteFiles()
{
  auto& card = m_slot_memcard[m_active_slot];
  if (!card)
    return;

  const auto selected_indices = GetSelectedFileIndices();
  if (selected_indices.empty())
    return;

  const QString text = tr("Do you want to delete the %n selected save file(s)?", "",
                          static_cast<int>(selected_indices.size()));
  const auto response = ModalMessageBox::question(this, tr("Question"), text);
  if (response != QMessageBox::Yes)
    return;

  for (const u8 index : selected_indices)
  {
    if (card->RemoveFile(index) != Memcard::GCMemcardRemoveFileRetVal::SUCCESS)
    {
      ModalMessageBox::warning(this, tr("Remove Failed"), tr("Failed to remove file."));
      break;
    }
  }

  if (!card->Save())
  {
    ModalMessageBox::warning(this, tr("Remove Failed"),
                             tr("Failed to write modified memory card to disk."));
  }

  UpdateSlotTable(m_active_slot);
  UpdateActions();
}

void GCMemcardManager::FixChecksums()
{
  auto& memcard = m_slot_memcard[m_active_slot];
  memcard->FixChecksums();

  if (!memcard->Save())
  {
    ModalMessageBox::warning(this, tr("Fix Checksums Failed"),
                             tr("Failed to write modified memory card to disk."));
  }
}

void GCMemcardManager::CreateNewCard(Slot slot)
{
  GCMemcardCreateNewDialog dialog(this);
  SetQWidgetWindowDecorations(&dialog);
  if (dialog.exec() == QDialog::Accepted)
    m_slot_file_edit[slot]->setText(QString::fromStdString(dialog.GetMemoryCardPath()));
}

void GCMemcardManager::DrawIcons()
{
  const int column = COLUMN_INDEX_ICON;
  for (Slot slot : MEMCARD_SLOTS)
  {
    QTableWidget* table = m_slot_table[slot];
    const int row_count = table->rowCount();

    if (row_count <= 0)
      continue;

    const auto viewport = table->viewport();
    const int viewport_first_row = table->indexAt(viewport->rect().topLeft()).row();
    if (viewport_first_row >= row_count)
      continue;

    const int first_row = viewport_first_row < 0 ? 0 : viewport_first_row;
    const int viewport_last_row = table->indexAt(viewport->rect().bottomLeft()).row();
    const int last_row =
        viewport_last_row < 0 ? (row_count - 1) : std::min(viewport_last_row, row_count - 1);

    for (int row = first_row; row <= last_row; ++row)
    {
      auto* item = table->item(row, column);
      if (!item)
        continue;

      const u8 index = static_cast<u8>(item->data(Qt::UserRole).toInt());
      auto it = m_slot_active_icons[slot].find(index);
      if (it == m_slot_active_icons[slot].end())
        continue;

      const auto& icon = it->second;

      // this icon doesn't have an animation
      if (icon.m_frames.size() <= 1)
        continue;

      const u64 prev_time_in_animation = (m_current_frame - 1) % icon.m_frame_timing.size();
      const u8 prev_frame = icon.m_frame_timing[prev_time_in_animation];
      const u64 current_time_in_animation = m_current_frame % icon.m_frame_timing.size();
      const u8 current_frame = icon.m_frame_timing[current_time_in_animation];

      if (prev_frame == current_frame)
        continue;

      item->setData(Qt::DecorationRole, icon.m_frames[current_frame]);
    }
  }

  ++m_current_frame;
}

QPixmap GCMemcardManager::GetBannerFromSaveFile(int file_index, Slot slot)
{
  auto& memcard = m_slot_memcard[slot];

  auto pxdata = memcard->ReadBannerRGBA8(file_index);

  QImage image;
  if (pxdata)
  {
    image = QImage(reinterpret_cast<u8*>(pxdata->data()), Memcard::MEMORY_CARD_BANNER_WIDTH,
                   Memcard::MEMORY_CARD_BANNER_HEIGHT, QImage::Format_ARGB32);
  }

  return QPixmap::fromImage(image);
}

GCMemcardManager::IconAnimationData GCMemcardManager::GetIconFromSaveFile(int file_index, Slot slot)
{
  auto& memcard = m_slot_memcard[slot];

  IconAnimationData frame_data;

  const auto decoded_data = memcard->ReadAnimRGBA8(file_index);

  // Decode Save File Animation
  if (decoded_data && !decoded_data->empty())
  {
    frame_data.m_frames.reserve(decoded_data->size());

    for (size_t f = 0; f < decoded_data->size(); ++f)
    {
      QImage img(reinterpret_cast<const u8*>((*decoded_data)[f].image_data.data()),
                 Memcard::MEMORY_CARD_ICON_WIDTH, Memcard::MEMORY_CARD_ICON_HEIGHT,
                 QImage::Format_ARGB32);
      frame_data.m_frames.push_back(QPixmap::fromImage(img));
      for (int i = 0; i < (*decoded_data)[f].delay; ++i)
      {
        frame_data.m_frame_timing.push_back(static_cast<u8>(f));
      }
    }

    const bool is_pingpong = memcard->DEntry_IsPingPong(file_index);
    if (is_pingpong && decoded_data->size() >= 3)
    {
      // if the animation 'ping-pongs' between start and end then the animation frame order is
      // something like 'abcdcbabcdcba' instead of the usual 'abcdabcdabcd'
      // to display that correctly just append all except the first and last frame in reverse order
      // at the end of the animation
      for (size_t f = decoded_data->size() - 2; f > 0; --f)
      {
        for (int i = 0; i < (*decoded_data)[f].delay; ++i)
        {
          frame_data.m_frame_timing.push_back(static_cast<u8>(f));
        }
      }
    }
  }
  else
  {
    // No Animation found, use an empty placeholder instead.
    frame_data.m_frames.emplace_back();
    frame_data.m_frame_timing.push_back(0);
  }

  return frame_data;
}

QString GCMemcardManager::GetErrorMessagesForErrorCode(const Memcard::GCMemcardErrorCode& code)
{
  QStringList sl;

  if (code.Test(Memcard::GCMemcardValidityIssues::FAILED_TO_OPEN))
    sl.push_back(tr("Couldn't open file."));

  if (code.Test(Memcard::GCMemcardValidityIssues::IO_ERROR))
    sl.push_back(tr("Couldn't read file."));

  if (code.Test(Memcard::GCMemcardValidityIssues::INVALID_CARD_SIZE))
    sl.push_back(tr("Filesize does not match any known GameCube Memory Card size."));

  if (code.Test(Memcard::GCMemcardValidityIssues::MISMATCHED_CARD_SIZE))
    sl.push_back(tr("Filesize in header mismatches actual card size."));

  if (code.Test(Memcard::GCMemcardValidityIssues::INVALID_CHECKSUM))
    sl.push_back(tr("Invalid checksums."));

  if (code.Test(Memcard::GCMemcardValidityIssues::FREE_BLOCK_MISMATCH))
    sl.push_back(tr("Mismatch between free block count in header and actually unused blocks."));

  if (code.Test(Memcard::GCMemcardValidityIssues::DIR_BAT_INCONSISTENT))
    sl.push_back(tr("Mismatch between internal data structures."));

  if (code.Test(Memcard::GCMemcardValidityIssues::DATA_IN_UNUSED_AREA))
    sl.push_back(tr("Data in area of file that should be unused."));

  if (sl.empty())
    return tr("No errors.");

  return sl.join(QLatin1Char{'\n'});
}

QString GCMemcardManager::GetErrorMessageForErrorCode(Memcard::ReadSavefileErrorCode code)
{
  switch (code)
  {
  case Memcard::ReadSavefileErrorCode::OpenFileFail:
    return tr("Failed to open file.");
  case Memcard::ReadSavefileErrorCode::IOError:
    return tr("Failed to read from file.");
  case Memcard::ReadSavefileErrorCode::DataCorrupted:
    return tr("Data in unrecognized format or corrupted.");
  default:
    return tr("Unknown error.");
  }
}
