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
#include <QTableWidget>
#include <QTimer>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

constexpr u32 BANNER_WIDTH = 96;
constexpr u32 ANIM_FRAME_WIDTH = 32;
constexpr u32 IMAGE_HEIGHT = 32;
constexpr u32 ANIM_MAX_FRAMES = 8;
constexpr float ROW_HEIGHT = 28;

GCMemcardManager::GCMemcardManager(QWidget* parent) : QDialog(parent)
{
  CreateWidgets();
  ConnectWidgets();

  SetActiveSlot(0);
  UpdateActions();

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &GCMemcardManager::DrawIcons);

  m_timer->start(1000 / 8);

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
  connect(m_select_button, &QPushButton::pressed, this, [this] { SetActiveSlot(!m_active_slot); });
  connect(m_export_button, &QPushButton::pressed, this, [this] { ExportFiles(true); });
  connect(m_export_all_button, &QPushButton::pressed, this, &GCMemcardManager::ExportAllFiles);
  connect(m_delete_button, &QPushButton::pressed, this, &GCMemcardManager::DeleteFiles);
  connect(m_import_button, &QPushButton::pressed, this, &GCMemcardManager::ImportFile);
  connect(m_copy_button, &QPushButton::pressed, this, &GCMemcardManager::CopyFiles);
  connect(m_fix_checksums_button, &QPushButton::pressed, this, &GCMemcardManager::FixChecksums);

  for (int slot = 0; slot < SLOT_COUNT; slot++)
  {
    connect(m_slot_file_edit[slot], &QLineEdit::textChanged, this,
            [this, slot](const QString& path) { SetSlotFile(slot, path); });
    connect(m_slot_file_button[slot], &QPushButton::clicked, this,
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

  auto create_item = [](const QString string = QStringLiteral("")) {
    QTableWidgetItem* item = new QTableWidgetItem(string);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
  };

  auto strip_garbage = [](const std::string s) {
    auto offset = s.find('\0');
    if (offset == std::string::npos)
      offset = s.length();

    return s.substr(0, offset);
  };

  for (int i = 0; i < memcard->GetNumFiles(); i++)
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

    auto frames = GetIconFromSaveFile(file_index, slot);
    auto* icon = new QTableWidgetItem;
    icon->setData(Qt::DecorationRole, frames[0]);

    std::optional<DEntry> entry = memcard->GetDEntry(file_index);

    // TODO: This is wrong, the animation speed is not static and is already correctly calculated in
    // GetIconFromSaveFile(), just not returned
    const u16 animation_speed = entry ? entry->m_animation_speed : 1;
    const auto speed = (((animation_speed >> 8) & 1) << 2) + (animation_speed & 1);

    m_slot_active_icons[slot].push_back({speed, frames});

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
  auto memcard = std::make_unique<GCMemcard>(path.toStdString());

  if (!memcard->IsValid())
    return;

  m_slot_file_edit[slot]->setText(path);
  m_slot_memcard[slot] = std::move(memcard);

  UpdateSlotTable(slot);
  UpdateActions();
}

void GCMemcardManager::SetSlotFileInteractive(int slot)
{
  QString path = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this,
      slot == 0 ? tr("Set memory card file for Slot A") : tr("Set memory card file for Slot B"),
      QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
      tr("GameCube Memory Cards (*.raw *.gcp)")));
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
      this, tr("Export Save File"), QString::fromStdString(File::GetUserPath(D_GCUSER_IDX)),
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
  m_current_frame++;
  m_current_frame %= 15;

  const auto column = 3;
  for (int slot = 0; slot < SLOT_COUNT; slot++)
  {
    int row = 0;
    for (auto& icon : m_slot_active_icons[slot])
    {
      int frame = (m_current_frame / 3 - icon.first) % icon.second.size();

      auto* item = new QTableWidgetItem;
      item->setData(Qt::DecorationRole, icon.second[frame]);
      item->setFlags(item->flags() ^ Qt::ItemIsEditable);

      m_slot_table[slot]->setItem(row, column, item);
      row++;
    }
  }
}

QPixmap GCMemcardManager::GetBannerFromSaveFile(int file_index, int slot)
{
  auto& memcard = m_slot_memcard[slot];

  std::vector<u32> pxdata(BANNER_WIDTH * IMAGE_HEIGHT);

  QImage image;
  if (memcard->ReadBannerRGBA8(file_index, pxdata.data()))
  {
    image = QImage(reinterpret_cast<u8*>(pxdata.data()), BANNER_WIDTH, IMAGE_HEIGHT,
                   QImage::Format_ARGB32);
  }

  return QPixmap::fromImage(image);
}

std::vector<QPixmap> GCMemcardManager::GetIconFromSaveFile(int file_index, int slot)
{
  auto& memcard = m_slot_memcard[slot];

  std::vector<u32> pxdata(BANNER_WIDTH * IMAGE_HEIGHT);
  std::vector<u8> anim_delay(ANIM_MAX_FRAMES);
  std::vector<u32> anim_data(ANIM_FRAME_WIDTH * IMAGE_HEIGHT * ANIM_MAX_FRAMES);

  std::vector<QPixmap> frame_pixmaps;

  u32 num_frames = memcard->ReadAnimRGBA8(file_index, anim_data.data(), anim_delay.data());

  // Decode Save File Animation
  if (num_frames > 0)
  {
    u32 frames = BANNER_WIDTH / ANIM_FRAME_WIDTH;

    if (num_frames < frames)
    {
      frames = num_frames;

      // Clear unused frame's pixels from the buffer.
      std::fill(pxdata.begin(), pxdata.end(), 0);
    }

    for (u32 f = 0; f < frames; ++f)
    {
      for (u32 y = 0; y < IMAGE_HEIGHT; ++y)
      {
        for (u32 x = 0; x < ANIM_FRAME_WIDTH; ++x)
        {
          // NOTE: pxdata is stacked horizontal, anim_data is stacked vertical
          pxdata[y * BANNER_WIDTH + f * ANIM_FRAME_WIDTH + x] =
              anim_data[f * ANIM_FRAME_WIDTH * IMAGE_HEIGHT + y * IMAGE_HEIGHT + x];
        }
      }
    }
    QImage anims(reinterpret_cast<u8*>(pxdata.data()), BANNER_WIDTH, IMAGE_HEIGHT,
                 QImage::Format_ARGB32);

    for (u32 f = 0; f < frames; f++)
    {
      frame_pixmaps.push_back(
          QPixmap::fromImage(anims.copy(ANIM_FRAME_WIDTH * f, 0, ANIM_FRAME_WIDTH, IMAGE_HEIGHT)));
    }
  }
  else
  {
    // No Animation found, use an empty placeholder instead.
    frame_pixmaps.push_back(QPixmap());
  }

  return frame_pixmaps;
}
