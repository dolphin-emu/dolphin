// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/ConvertDialog.h"

#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QErrorMessage>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"

static bool CompressCB(const std::string& text, float percent, void* ptr)
{
  if (ptr == nullptr)
    return false;

  auto* progress_dialog = static_cast<ParallelProgressDialog*>(ptr);

  progress_dialog->SetValue(percent * 100);
  return !progress_dialog->WasCanceled();
}

ConvertDialog::ConvertDialog(QList<std::shared_ptr<const UICommon::GameFile>> files,
                             QWidget* parent)
    : QDialog(parent), m_files(std::move(files))
{
  ASSERT(!m_files.empty());

  setWindowTitle(tr("Convert"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QGridLayout* grid_layout = new QGridLayout;
  grid_layout->setColumnStretch(1, 1);

  m_format = new QComboBox;
  AddToFormatComboBox(QStringLiteral("ISO"), DiscIO::BlobType::PLAIN);
  AddToFormatComboBox(QStringLiteral("GCZ"), DiscIO::BlobType::GCZ);
  grid_layout->addWidget(new QLabel(tr("Format:")), 0, 0);
  grid_layout->addWidget(m_format, 0, 1);

  m_block_size = new QComboBox;
  grid_layout->addWidget(new QLabel(tr("Block Size:")), 1, 0);
  grid_layout->addWidget(m_block_size, 1, 1);

  m_scrub = new QCheckBox;
  grid_layout->addWidget(new QLabel(tr("Remove Junk Data (Irreversible):")), 2, 0);
  grid_layout->addWidget(m_scrub, 2, 1);
  m_scrub->setEnabled(
      std::none_of(m_files.begin(), m_files.end(), std::mem_fn(&UICommon::GameFile::IsDatelDisc)));

  QPushButton* convert_button = new QPushButton(tr("Convert"));

  QVBoxLayout* options_layout = new QVBoxLayout;
  options_layout->addLayout(grid_layout);
  options_layout->addWidget(convert_button);
  QGroupBox* options_group = new QGroupBox(tr("Options"));
  options_group->setLayout(options_layout);

  QLabel* info_text =
      new QLabel(tr("ISO: A simple and robust format which is supported by many programs. "
                    "It takes up more space than any other format.\n\n"
                    "GCZ: A basic compressed format which is compatible with most versions of "
                    "Dolphin and some other programs. It can't efficiently compress junk data "
                    "(unless removed) or encrypted Wii data."));
  info_text->setWordWrap(true);

  QVBoxLayout* info_layout = new QVBoxLayout;
  info_layout->addWidget(info_text);
  QGroupBox* info_group = new QGroupBox(tr("Info"));
  info_group->setLayout(info_layout);

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addWidget(options_group);
  main_layout->addWidget(info_group);

  setLayout(main_layout);

  connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ConvertDialog::OnFormatChanged);
  connect(convert_button, &QPushButton::clicked, this, &ConvertDialog::Convert);

  OnFormatChanged();
}

void ConvertDialog::AddToFormatComboBox(const QString& name, DiscIO::BlobType format)
{
  if (std::all_of(m_files.begin(), m_files.end(),
                  [format](const auto& file) { return file->GetBlobType() == format; }))
  {
    return;
  }

  m_format->addItem(name, static_cast<int>(format));
}

void ConvertDialog::AddToBlockSizeComboBox(int size)
{
  m_block_size->addItem(QString::fromStdString(UICommon::FormatSize(size, 0)), size);
}

void ConvertDialog::OnFormatChanged()
{
  // Because DVD timings are emulated as if we can't read less than an entire ECC block at once
  // (32 KiB - 0x8000), there is little reason to use a block size smaller than that.
  constexpr int MIN_BLOCK_SIZE = 0x8000;

  // For performance reasons, blocks shouldn't be too large.
  // 2 MiB (0x200000) was picked because it is the smallest block size supported by WIA.
  constexpr int MAX_BLOCK_SIZE = 0x200000;

  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(m_format->currentData().toInt());

  m_block_size->clear();
  switch (format)
  {
  case DiscIO::BlobType::GCZ:
  {
    m_block_size->setEnabled(true);

    // In order for versions of Dolphin prior to 5.0-11893 to be able to convert a GCZ file
    // to ISO without messing up the final part of the file in some way, the file size
    // must be an integer multiple of the block size (fixed in 3aa463c) and must not be
    // an integer multiple of the block size multiplied by 32 (fixed in 26b21e3).

    const auto block_size_ok = [this](int block_size) {
      return std::all_of(m_files.begin(), m_files.end(), [block_size](const auto& file) {
        constexpr u64 BLOCKS_PER_BUFFER = 32;
        const u64 file_size = file->GetVolumeSize();
        return file_size % block_size == 0 && file_size % (block_size * BLOCKS_PER_BUFFER) != 0;
      });
    };

    // Add all block sizes in the normal range that do not cause problems
    for (int block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2)
    {
      if (block_size_ok(block_size))
        AddToBlockSizeComboBox(block_size);
    }

    // If we didn't find a good block size, pick the block size which was hardcoded
    // in older versions of Dolphin. That way, at least we're not worse than older versions.
    if (m_block_size->count() == 0)
    {
      constexpr int FALLBACK_BLOCK_SIZE = 0x4000;
      if (!block_size_ok(FALLBACK_BLOCK_SIZE))
      {
        ERROR_LOG(MASTER_LOG, "Failed to find a block size which does not cause problems "
                              "when decompressing using an old version of Dolphin");
      }
      AddToBlockSizeComboBox(FALLBACK_BLOCK_SIZE);
    }

    break;
  }
  default:
    m_block_size->setEnabled(false);
    break;
  }
}

bool ConvertDialog::ShowAreYouSureDialog(const QString& text)
{
  ModalMessageBox warning(this);
  warning.setIcon(QMessageBox::Warning);
  warning.setWindowTitle(tr("Confirm"));
  warning.setText(tr("Are you sure?"));
  warning.setInformativeText(text);
  warning.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

  return warning.exec() == QMessageBox::Yes;
}

void ConvertDialog::Convert()
{
  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(m_format->currentData().toInt());
  const int block_size = m_block_size->currentData().toInt();
  const bool scrub = m_scrub->isChecked();

  if (scrub && format == DiscIO::BlobType::PLAIN)
  {
    if (!ShowAreYouSureDialog(tr("Removing junk data does not save any space when converting to "
                                 "ISO (unless you package the ISO file in a compressed file format "
                                 "such as ZIP afterwards). Do you want to continue anyway?")))
    {
      return;
    }
  }

  if (!scrub && format == DiscIO::BlobType::GCZ &&
      std::any_of(m_files.begin(), m_files.end(), [](const auto& file) {
        return file->GetPlatform() == DiscIO::Platform::WiiDisc && !file->IsDatelDisc();
      }))
  {
    if (!ShowAreYouSureDialog(tr("Converting Wii disc images to GCZ without removing junk data "
                                 "does not save any noticeable amount of space compared to "
                                 "converting to ISO. Do you want to continue anyway?")))
    {
      return;
    }
  }

  QString extension;
  QString filter;
  switch (format)
  {
  case DiscIO::BlobType::PLAIN:
    extension = QStringLiteral(".iso");
    filter = tr("Uncompressed GC/Wii images (*.iso *.gcm)");
    break;
  case DiscIO::BlobType::GCZ:
    extension = QStringLiteral(".gcz");
    filter = tr("Compressed GC/Wii images (*.gcz)");
    break;
  default:
    ASSERT(false);
    return;
  }

  QString dst_dir;
  QString dst_path;

  if (m_files.size() > 1)
  {
    dst_dir = QFileDialog::getExistingDirectory(
        this, tr("Select where you want to save the converted images"),
        QFileInfo(QString::fromStdString(m_files[0]->GetFilePath())).dir().absolutePath());

    if (dst_dir.isEmpty())
      return;
  }
  else
  {
    dst_path = QFileDialog::getSaveFileName(
        this, tr("Select where you want to save the converted image"),
        QFileInfo(QString::fromStdString(m_files[0]->GetFilePath()))
            .dir()
            .absoluteFilePath(
                QFileInfo(QString::fromStdString(m_files[0]->GetFilePath())).completeBaseName())
            .append(extension),
        filter);

    if (dst_path.isEmpty())
      return;
  }

  for (const auto& file : m_files)
  {
    const auto original_path = file->GetFilePath();
    if (m_files.size() > 1)
    {
      dst_path =
          QDir(dst_dir)
              .absoluteFilePath(QFileInfo(QString::fromStdString(original_path)).completeBaseName())
              .append(extension);
      QFileInfo dst_info = QFileInfo(dst_path);
      if (dst_info.exists())
      {
        ModalMessageBox confirm_replace(this);
        confirm_replace.setIcon(QMessageBox::Warning);
        confirm_replace.setWindowTitle(tr("Confirm"));
        confirm_replace.setText(tr("The file %1 already exists.\n"
                                   "Do you wish to replace it?")
                                    .arg(dst_info.fileName()));
        confirm_replace.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        if (confirm_replace.exec() == QMessageBox::No)
          continue;
      }
    }

    ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Abort"), 0, 100, this);
    progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
    progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));

    if (m_files.size() > 1)
    {
      progress_dialog.GetRaw()->setLabelText(
          tr("Converting...") + QLatin1Char{'\n'} +
          QFileInfo(QString::fromStdString(original_path)).fileName());
    }

    std::unique_ptr<DiscIO::BlobReader> blob_reader;
    bool scrub_current_file = scrub;

    if (scrub_current_file)
    {
      blob_reader = DiscIO::ScrubbedBlob::Create(original_path);
      if (!blob_reader)
      {
        const int result =
            ModalMessageBox::warning(this, tr("Question"),
                                     tr("Failed to remove junk data from file \"%1\".\n\n"
                                        "Would you like to convert it without removing junk data?")
                                         .arg(QString::fromStdString(original_path)),
                                     QMessageBox::Ok | QMessageBox::Abort);

        if (result == QMessageBox::Ok)
          scrub_current_file = false;
        else
          return;
      }
    }

    if (!scrub_current_file)
      blob_reader = DiscIO::CreateBlobReader(original_path);

    if (!blob_reader)
    {
      QErrorMessage(this).showMessage(
          tr("Failed to open the input file \"%1\".").arg(QString::fromStdString(original_path)));
    }
    else
    {
      std::future<bool> good;

      if (format == DiscIO::BlobType::PLAIN)
      {
        good = std::async(std::launch::async, [&] {
          const bool good =
              DiscIO::ConvertToPlain(blob_reader.get(), original_path, dst_path.toStdString(),
                                     &CompressCB, &progress_dialog);
          progress_dialog.Reset();
          return good;
        });
      }
      else if (format == DiscIO::BlobType::GCZ)
      {
        good = std::async(std::launch::async, [&] {
          const bool good =
              DiscIO::ConvertToGCZ(blob_reader.get(), original_path, dst_path.toStdString(),
                                   file->GetPlatform() == DiscIO::Platform::WiiDisc ? 1 : 0,
                                   block_size, &CompressCB, &progress_dialog);
          progress_dialog.Reset();
          return good;
        });
      }

      progress_dialog.GetRaw()->exec();
      if (!good.get())
      {
        QErrorMessage(this).showMessage(tr("Dolphin failed to complete the requested action."));
        return;
      }
    }
  }

  ModalMessageBox::information(this, tr("Success"),
                               tr("Successfully converted %n image(s).", "", m_files.size()));

  close();
}
