// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/GCMemcardManager.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

constexpr u32 BANNER_WIDTH = 96;
constexpr u32 BANNER_HEIGHT = 32;
constexpr u32 ICON_WIDTH = 32;
constexpr u32 ICON_HEIGHT = 32;
constexpr u32 ANIM_MAX_FRAMES = 8;
constexpr float ROW_HEIGHT = 28;

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

  SetActiveSlot(0);
  UpdateActions();

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &GCMemcardManager::DrawIcons);

  // individual frames of icon animations can stay on screen for 4, 8, or 12 frames at 60 FPS,
  // which means the fastest animation and common denominator is 15 FPS or 66 milliseconds per frame
  m_timer->start(1000 / 15);

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
  m_select_button = new QPushButton;
  m_copy_button = new QPushButton;

  // Contents will be set by their appropriate functions
  m_delete_button = new QPushButton(tr("&Delete"));
  m_export_button = new QPushButton(tr("&Export..."));
  m_export_all_button = new QPushButton(tr("Export &All..."));
  m_import_button = new QPushButton(tr("&Import..."));
  m_fix_checksums_button = new QPushButton(tr("Fix Checksums"));

  auto* layout = new QGridLayout;

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    m_slot_group[i] = new QGroupBox(i == 0 ? tr("Slot A") : tr("Slot B"));
    m_slot_file_edit[i] = new QLineEdit;
    m_slot_file_button[i] = new QPushButton(tr("&Browse..."));
    m_slot_table[i] = new QTableWidget;
    m_slot_stat_label[i] = new QLabel;

    m_slot_table[i]->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_slot_table[i]->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_slot_table[i]->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_slot_table[i]->horizontalHeader()->setHighlightSections(false);
    m_slot_table[i]->verticalHeader()->hide();
    m_slot_table[i]->setShowGrid(false);

    auto* slot_layout = new QGridLayout;
    m_slot_group[i]->setLayout(slot_layout);

    slot_layout->addWidget(m_slot_file_edit[i], 0, 0);
    slot_layout->addWidget(m_slot_file_button[i], 0, 1);
    slot_layout->addWidget(m_slot_table[i], 1, 0, 1, 2);
    slot_layout->addWidget(m_slot_stat_label[i], 2, 0);

    layout->addWidget(m_slot_group[i], 0, i * 2, 9, 1);

    UpdateSlotTable(i);
  }

  layout->addWidget(m_select_button, 1, 1);
  layout->addWidget(m_copy_button, 2, 1);
  layout->addWidget(m_delete_button, 3, 1);
  layout->addWidget(m_export_button, 4, 1);
  layout->addWidget(m_export_all_button, 5, 1);
  layout->addWidget(m_import_button, 6, 1);
  layout->addWidget(m_fix_checksums_button, 7, 1);
  layout->addWidget(m_button_box, 9, 2);

  setLayout(layout);
}

void GCMemcardManager::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_select_button, &QPushButton::clicked, [this] { SetActiveSlot(!m_active_slot); });
  connect(m_export_button, &QPushButton::clicked, [this] { ExportFiles(true); });
  connect(m_export_all_button, &QPushButton::clicked, this, &GCMemcardManager::ExportAllFiles);
  connect(m_delete_button, &QPushButton::clicked, this, &GCMemcardManager::DeleteFiles);
  connect(m_import_button, &QPushButton::clicked, this, &GCMemcardManager::ImportFile);
  connect(m_copy_button, &QPushButton::clicked, this, &GCMemcardManager::CopyFiles);
  connect(m_fix_checksums_button, &QPushButton::clicked, this, &GCMemcardManager::FixChecksums);

  for (int slot = 0; slot < SLOT_COUNT; slot++)
  {
    connect(m_slot_file_edit[slot], &QLineEdit::textChanged,
            [this, slot](const QString& path) { SetSlotFile(slot, path); });
    connect(m_slot_file_button[slot], &QPushButton::clicked,
            [this, slot] { SetSlotFileInteractive(slot); });
    connect(m_slot_table[slot], &QTableWidget::itemSelectionChanged, this,
            &GCMemcardManager::UpdateActions);
  }
}

void GCMemcardManager::SetActiveSlot(int slot)
{
  for (int i = 0; i < SLOT_COUNT; i++)
    m_slot_table[i]->setEnabled(i == slot);

  m_select_button->setText(slot == 0 ? tr("Switch to B") : tr("Switch to A"));
  m_copy_button->setText(slot == 0 ? tr("Copy to B") : tr("Copy to A"));

  m_active_slot = slot;

  UpdateSlotTable(slot);
  UpdateActions();
}

void GCMemcardManager::UpdateSlotTable(int slot)
{
  m_slot_active_icons[slot].clear();
  m_slot_table[slot]->clear();
  m_slot_table[slot]->setColumnCount(6);
  m_slot_table[slot]->verticalHeader()->setDefaultSectionSize(ROW_HEIGHT);
  m_slot_table[slot]->verticalHeader()->setDefaultSectionSize(QHeaderView::Fixed);
  m_slot_table[slot]->setHorizontalHeaderLabels(
      {tr("Banner"), tr("Title"), tr("Comment"), tr("Icon"), tr("Blocks"), tr("First Block")});

  if (m_slot_memcard[slot] == nullptr)
    return;

  auto& memcard = m_slot_memcard[slot];
  auto* table = m_slot_table[slot];

  const auto create_item = [](const QString& string = {}) {
    QTableWidgetItem* item = new QTableWidgetItem(string);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
  };

  const auto strip_garbage = [](const std::string& s) {
    auto offset = s.find('\0');
    if (offset == std::string::npos)
      offset = s.length();

    return s.substr(0, offset);
  };

  const u8 num_files = memcard->GetNumFiles();
  m_slot_active_icons[slot].reserve(num_files);
  for (int i = 0; i < num_files; i++)
  {
    int file_index = memcard->GetFileIndex(i);
    table->setRowCount(i + 1);

    auto const string_decoder = memcard->IsShiftJIS() ? SHIFTJISToUTF8 : CP1252ToUTF8;

    QString title =
        QString::fromStdString(strip_garbage(string_decoder(memcard->GetSaveComment1(file_index))));
    QString comment =
        QString::fromStdString(strip_garbage(string_decoder(memcard->GetSaveComment2(file_index))));
    QString blocks = QStringLiteral("%1").arg(memcard->DEntry_BlockCount(file_index));
    QString block_count = QStringLiteral("%1").arg(memcard->DEntry_FirstBlock(file_index));

    auto* banner = new QTableWidgetItem;
    banner->setData(Qt::DecorationRole, GetBannerFromSaveFile(file_index, slot));
    banner->setFlags(banner->flags() ^ Qt::ItemIsEditable);

    auto icon_data = GetIconFromSaveFile(file_index, slot);
    auto* icon = new QTableWidgetItem;
    icon->setData(Qt::DecorationRole, icon_data.m_frames[0]);

    std::optional<DEntry> entry = memcard->GetDEntry(file_index);

    m_slot_active_icons[slot].emplace_back(std::move(icon_data));

    table->setItem(i, 0, banner);
    table->setItem(i, 1, create_item(title));
    table->setItem(i, 2, create_item(comment));
    table->setItem(i, 3, icon);
    table->setItem(i, 4, create_item(blocks));
    table->setItem(i, 5, create_item(block_count));
    table->resizeRowToContents(i);
  }

  m_slot_stat_label[slot]->setText(tr("%1 Free Blocks; %2 Free Dir Entries")
                                       .arg(memcard->GetFreeBlocks())
                                       .arg(DIRLEN - memcard->GetNumFiles()));
}

void GCMemcardManager::UpdateActions()
{
  auto selection = m_slot_table[m_active_slot]->selectedItems();
  bool have_selection = selection.count();
  bool have_memcard = m_slot_memcard[m_active_slot] != nullptr;
  bool have_memcard_other = m_slot_memcard[!m_active_slot] != nullptr;

  m_copy_button->setEnabled(have_selection && have_memcard_other);
  m_export_button->setEnabled(have_selection);
  m_export_all_button->setEnabled(have_memcard);
  m_import_button->setEnabled(have_memcard);
  m_delete_button->setEnabled(have_selection);
  m_fix_checksums_button->setEnabled(have_memcard);
}

void GCMemcardManager::SetSlotFile(int slot, QString path)
{
  auto [error_code, memcard] = GCMemcard::Open(path.toStdString());

  if (!error_code.HasCriticalErrors() && memcard && memcard->IsValid())
  {
    m_slot_file_edit[slot]->setText(path);
    m_slot_memcard[slot] = std::make_unique<GCMemcard>(std::move(*memcard));
  }
  else
  {
    m_slot_memcard[slot] = nullptr;
    ModalMessageBox::critical(
        this, tr("Error"),
        tr("Failed opening memory card:\n%1").arg(GetErrorMessagesForErrorCode(error_code)));
  }

  UpdateSlotTable(slot);
  UpdateActions();
}

void GCMemcardManager::SetSlotFileInteractive(int slot)
{
  QString path = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this,
      slot == 0 ? tr("Set memory card file for Slot A") : tr("Set memory card file for Slot B"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)") + QStringLiteral(";;") + tr("All Files (*)")));
  if (!path.isEmpty())
    m_slot_file_edit[slot]->setText(path);
}

void GCMemcardManager::ExportFiles(bool prompt)
{
  auto selection = m_slot_table[m_active_slot]->selectedItems();
  auto& memcard = m_slot_memcard[m_active_slot];

  auto count = selection.count() / m_slot_table[m_active_slot]->columnCount();

  for (int i = 0; i < count; i++)
  {
    auto sel = selection[i * m_slot_table[m_active_slot]->columnCount()];
    int file_index = memcard->GetFileIndex(m_slot_table[m_active_slot]->row(sel));

    std::string gci_filename;
    if (!memcard->GCI_FileName(file_index, gci_filename))
      return;

    QString path;
    if (prompt)
    {
      path = QFileDialog::getSaveFileName(
          this, tr("Export Save File"),
          QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)) +
              QStringLiteral("/%1").arg(QString::fromStdString(gci_filename)),
          tr("Native GCI File (*.gci)") + QStringLiteral(";;") +
              tr("MadCatz Gameshark files(*.gcs)") + QStringLiteral(";;") +
              tr("Datel MaxDrive/Pro files(*.sav)"));

      if (path.isEmpty())
        return;
    }
    else
    {
      path = QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)) +
             QStringLiteral("/%1").arg(QString::fromStdString(gci_filename));
    }

    // TODO: This is obviously intended to check for success instead.
    const auto exportRetval = memcard->ExportGci(file_index, path.toStdString(), "");
    if (exportRetval == GCMemcardExportFileRetVal::UNUSED)
    {
      File::Delete(path.toStdString());
    }
  }

  QString text = count == 1 ? tr("Successfully exported the save file.") :
                              tr("Successfully exported the %1 save files.").arg(count);
  ModalMessageBox::information(this, tr("Success"), text);
}

void GCMemcardManager::ExportAllFiles()
{
  // This is nothing but a thin wrapper around ExportFiles()
  m_slot_table[m_active_slot]->selectAll();
  ExportFiles(false);
}

void GCMemcardManager::ImportFile()
{
  QString path = QFileDialog::getOpenFileName(
      this, tr("Import Save File"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("Native GCI File (*.gci)") + QStringLiteral(";;") + tr("MadCatz Gameshark files(*.gcs)") +
          QStringLiteral(";;") + tr("Datel MaxDrive/Pro files(*.sav)"));

  if (path.isEmpty())
    return;

  const auto result = m_slot_memcard[m_active_slot]->ImportGci(path.toStdString());

  if (result != GCMemcardImportFileRetVal::SUCCESS)
  {
    ModalMessageBox::critical(this, tr("Import failed"), tr("Failed to import \"%1\".").arg(path));
    return;
  }

  if (!m_slot_memcard[m_active_slot]->Save())
    PanicAlertT("File write failed");

  UpdateSlotTable(m_active_slot);
}

void GCMemcardManager::CopyFiles()
{
  auto selection = m_slot_table[m_active_slot]->selectedItems();
  auto& memcard = m_slot_memcard[m_active_slot];

  auto count = selection.count() / m_slot_table[m_active_slot]->columnCount();

  for (int i = 0; i < count; i++)
  {
    auto sel = selection[i * m_slot_table[m_active_slot]->columnCount()];
    int file_index = memcard->GetFileIndex(m_slot_table[m_active_slot]->row(sel));

    const auto result = m_slot_memcard[!m_active_slot]->CopyFrom(*memcard, file_index);

    if (result != GCMemcardImportFileRetVal::SUCCESS)
    {
      ModalMessageBox::warning(this, tr("Copy failed"), tr("Failed to copy file"));
    }
  }

  for (int i = 0; i < SLOT_COUNT; i++)
  {
    if (!m_slot_memcard[i]->Save())
      PanicAlertT("File write failed");

    UpdateSlotTable(i);
  }
}

void GCMemcardManager::DeleteFiles()
{
  auto selection = m_slot_table[m_active_slot]->selectedItems();
  auto& memcard = m_slot_memcard[m_active_slot];

  auto count = selection.count() / m_slot_table[m_active_slot]->columnCount();

  // Ask for confirmation if we are to delete multiple files
  if (count > 1)
  {
    QString text = count == 1 ? tr("Do you want to delete the selected save file?") :
                                tr("Do you want to delete the %1 selected save files?").arg(count);

    auto response = ModalMessageBox::question(this, tr("Question"), text);
    ;

    if (response == QMessageBox::Abort)
      return;
  }

  std::vector<int> file_indices;
  for (int i = 0; i < count; i++)
  {
    auto sel = selection[i * m_slot_table[m_active_slot]->columnCount()];
    file_indices.push_back(memcard->GetFileIndex(m_slot_table[m_active_slot]->row(sel)));
  }

  for (int file_index : file_indices)
  {
    if (memcard->RemoveFile(file_index) != GCMemcardRemoveFileRetVal::SUCCESS)
    {
      ModalMessageBox::warning(this, tr("Remove failed"), tr("Failed to remove file"));
    }
  }

  if (!memcard->Save())
  {
    PanicAlertT("File write failed");
  }
  else
  {
    ModalMessageBox::information(this, tr("Success"), tr("Successfully deleted files."));
  }

  UpdateSlotTable(m_active_slot);
  UpdateActions();
}

void GCMemcardManager::FixChecksums()
{
  auto& memcard = m_slot_memcard[m_active_slot];
  memcard->FixChecksums();

  if (!memcard->Save())
    PanicAlertT("File write failed");
}

void GCMemcardManager::DrawIcons()
{
  const auto column = 3;
  for (int slot = 0; slot < SLOT_COUNT; slot++)
  {
    int row = 0;
    for (const auto& icon : m_slot_active_icons[slot])
    {
      const u64 current_time_in_animation = m_current_frame % icon.m_frame_timing.size();
      const u8 current_frame = icon.m_frame_timing[current_time_in_animation];

      auto* item = new QTableWidgetItem;
      item->setData(Qt::DecorationRole, icon.m_frames[current_frame]);
      item->setFlags(item->flags() ^ Qt::ItemIsEditable);

      m_slot_table[slot]->setItem(row, column, item);
      row++;
    }
  }

  ++m_current_frame;
}

QPixmap GCMemcardManager::GetBannerFromSaveFile(int file_index, int slot)
{
  auto& memcard = m_slot_memcard[slot];

  std::vector<u32> pxdata(BANNER_WIDTH * BANNER_HEIGHT);

  QImage image;
  if (memcard->ReadBannerRGBA8(file_index, pxdata.data()))
  {
    image = QImage(reinterpret_cast<u8*>(pxdata.data()), BANNER_WIDTH, BANNER_HEIGHT,
                   QImage::Format_ARGB32);
  }

  return QPixmap::fromImage(image);
}

GCMemcardManager::IconAnimationData GCMemcardManager::GetIconFromSaveFile(int file_index, int slot)
{
  auto& memcard = m_slot_memcard[slot];

  std::vector<u8> anim_delay(ANIM_MAX_FRAMES);
  std::vector<u32> anim_data(ICON_WIDTH * ICON_HEIGHT * ANIM_MAX_FRAMES);

  IconAnimationData frame_data;

  const u32 num_frames = memcard->ReadAnimRGBA8(file_index, anim_data.data(), anim_delay.data());

  // Decode Save File Animation
  if (num_frames > 0)
  {
    frame_data.m_frames.reserve(num_frames);
    const u32 per_frame_offset = ICON_WIDTH * ICON_HEIGHT;
    for (u32 f = 0; f < num_frames; ++f)
    {
      QImage img(reinterpret_cast<u8*>(&anim_data[f * per_frame_offset]), ICON_WIDTH, ICON_HEIGHT,
                 QImage::Format_ARGB32);
      frame_data.m_frames.push_back(QPixmap::fromImage(img));
      for (int i = 0; i < anim_delay[f]; ++i)
      {
        frame_data.m_frame_timing.push_back(static_cast<u8>(f));
      }
    }

    const bool is_pingpong = memcard->DEntry_IsPingPong(file_index);
    if (is_pingpong && num_frames >= 3)
    {
      // if the animation 'ping-pongs' between start and end then the animation frame order is
      // something like 'abcdcbabcdcba' instead of the usual 'abcdabcdabcd'
      // to display that correctly just append all except the first and last frame in reverse order
      // at the end of the animation
      for (u32 f = num_frames - 2; f > 0; --f)
      {
        for (int i = 0; i < anim_delay[f]; ++i)
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
    frame_data.m_frame_timing.push_back(1);
  }

  return frame_data;
}

QString GCMemcardManager::GetErrorMessagesForErrorCode(const GCMemcardErrorCode& code)
{
  QStringList sl;

  if (code.Test(GCMemcardValidityIssues::FAILED_TO_OPEN))
    sl.push_back(tr("Couldn't open file."));

  if (code.Test(GCMemcardValidityIssues::IO_ERROR))
    sl.push_back(tr("Couldn't read file."));

  if (code.Test(GCMemcardValidityIssues::INVALID_CARD_SIZE))
    sl.push_back(tr("Filesize does not match any known GameCube Memory Card size."));

  if (code.Test(GCMemcardValidityIssues::MISMATCHED_CARD_SIZE))
    sl.push_back(tr("Filesize in header mismatches actual card size."));

  if (code.Test(GCMemcardValidityIssues::INVALID_CHECKSUM))
    sl.push_back(tr("Invalid checksums."));

  if (code.Test(GCMemcardValidityIssues::FREE_BLOCK_MISMATCH))
    sl.push_back(tr("Mismatch between free block count in header and actually unused blocks."));

  if (code.Test(GCMemcardValidityIssues::DIR_BAT_INCONSISTENT))
    sl.push_back(tr("Mismatch between internal data structures."));

  if (code.Test(GCMemcardValidityIssues::DATA_IN_UNUSED_AREA))
    sl.push_back(tr("Data in area of file that should be unused."));

  if (sl.empty())
    return QStringLiteral("No errors.");

  return sl.join(QStringLiteral("\n"));
}
