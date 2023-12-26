// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ConvertDialog.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
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
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DiscIO/WIABlob.h"
#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "UICommon/GameFile.h"
#include "UICommon/UICommon.h"

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
  m_format->addItem(QStringLiteral("ISO"), static_cast<int>(DiscIO::BlobType::PLAIN));
  m_format->addItem(QStringLiteral("GCZ"), static_cast<int>(DiscIO::BlobType::GCZ));
  m_format->addItem(QStringLiteral("WIA"), static_cast<int>(DiscIO::BlobType::WIA));
  m_format->addItem(QStringLiteral("RVZ"), static_cast<int>(DiscIO::BlobType::RVZ));
  if (std::all_of(m_files.begin(), m_files.end(),
                  [](const auto& file) { return file->GetBlobType() == DiscIO::BlobType::PLAIN; }))
  {
    m_format->setCurrentIndex(m_format->count() - 1);
  }
  grid_layout->addWidget(new QLabel(tr("Format:")), 0, 0);
  grid_layout->addWidget(m_format, 0, 1);

  m_block_size = new QComboBox;
  grid_layout->addWidget(new QLabel(tr("Block Size:")), 1, 0);
  grid_layout->addWidget(m_block_size, 1, 1);

  m_compression = new QComboBox;
  grid_layout->addWidget(new QLabel(tr("Compression:")), 2, 0);
  grid_layout->addWidget(m_compression, 2, 1);

  m_compression_level = new QComboBox;
  grid_layout->addWidget(new QLabel(tr("Compression Level:")), 3, 0);
  grid_layout->addWidget(m_compression_level, 3, 1);

  m_scrub = new QCheckBox;
  grid_layout->addWidget(new QLabel(tr("Remove Junk Data (Irreversible):")), 4, 0);
  grid_layout->addWidget(m_scrub, 4, 1);

  QPushButton* convert_button = new QPushButton(tr("Convert..."));

  QVBoxLayout* options_layout = new QVBoxLayout;
  options_layout->addLayout(grid_layout);
  options_layout->addWidget(convert_button);
  QGroupBox* options_group = new QGroupBox(tr("Options"));
  options_group->setLayout(options_layout);

  QLabel* info_text = new QLabel(
      tr("ISO: A simple and robust format which is supported by many programs. It takes up more "
         "space than any other format.\n\n"
         "GCZ: A basic compressed format which is compatible with most versions of Dolphin and "
         "some other programs. It can't efficiently compress junk data (unless removed) or "
         "encrypted Wii data.\n\n"
         "WIA: An advanced compressed format which is compatible with Dolphin 5.0-12188 and later, "
         "and a few other programs. It can efficiently compress encrypted Wii data, but not junk "
         "data (unless removed).\n\n"
         "RVZ: An advanced compressed format which is compatible with Dolphin 5.0-12188 and later. "
         "It can efficiently compress both junk data and encrypted Wii data."));
  info_text->setWordWrap(true);

  QVBoxLayout* info_layout = new QVBoxLayout;
  info_layout->addWidget(info_text);
  QGroupBox* info_group = new QGroupBox(tr("Info"));
  info_group->setLayout(info_layout);

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addWidget(options_group);
  main_layout->addWidget(info_group);

  setLayout(main_layout);

  connect(m_format, &QComboBox::currentIndexChanged, this, &ConvertDialog::OnFormatChanged);
  connect(m_compression, &QComboBox::currentIndexChanged, this,
          &ConvertDialog::OnCompressionChanged);
  connect(convert_button, &QPushButton::clicked, this, &ConvertDialog::Convert);

  OnFormatChanged();
  OnCompressionChanged();
}

void ConvertDialog::AddToBlockSizeComboBox(int size)
{
  m_block_size->addItem(QString::fromStdString(UICommon::FormatSize(size, 0)), size);

  // Select the default, or if it is not available, the size closest to it.
  // This code assumes that sizes get added to the combo box in increasing order.
  if (size <= DiscIO::GCZ_RVZ_PREFERRED_BLOCK_SIZE)
    m_block_size->setCurrentIndex(m_block_size->count() - 1);
}

void ConvertDialog::AddToCompressionComboBox(const QString& name,
                                             DiscIO::WIARVZCompressionType type)
{
  m_compression->addItem(name, static_cast<int>(type));
}

void ConvertDialog::AddToCompressionLevelComboBox(int level)
{
  m_compression_level->addItem(QString::number(level), level);
}

void ConvertDialog::OnFormatChanged()
{
  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(m_format->currentData().toInt());

  m_block_size->clear();
  m_compression->clear();

  // Populate m_block_size
  switch (format)
  {
  case DiscIO::BlobType::GCZ:
  {
    // To support legacy versions of dolphin, we have to check the GCZ block size
    // See DiscIO::IsGCZBlockSizeLegacyCompatible() for details
    const auto block_size_ok = [this](int block_size) {
      return std::all_of(m_files.begin(), m_files.end(), [block_size](const auto& file) {
        return DiscIO::IsGCZBlockSizeLegacyCompatible(block_size, file->GetVolumeSize());
      });
    };

    // Add all block sizes in the normal range that do not cause problems
    for (int block_size = DiscIO::PREFERRED_MIN_BLOCK_SIZE;
         block_size <= DiscIO::PREFERRED_MAX_BLOCK_SIZE; block_size *= 2)
    {
      if (block_size_ok(block_size))
        AddToBlockSizeComboBox(block_size);
    }

    // If we didn't find a good block size, pick the block size which was hardcoded
    // in older versions of Dolphin. That way, at least we're not worse than older versions.
    if (m_block_size->count() == 0)
    {
      if (!block_size_ok(DiscIO::GCZ_FALLBACK_BLOCK_SIZE))
      {
        ERROR_LOG_FMT(MASTER_LOG, "Failed to find a block size which does not cause problems "
                                  "when decompressing using an old version of Dolphin");
      }
      AddToBlockSizeComboBox(DiscIO::GCZ_FALLBACK_BLOCK_SIZE);
    }

    break;
  }
  case DiscIO::BlobType::WIA:
    m_block_size->setEnabled(true);

    // This is the smallest block size supported by WIA. For performance, larger sizes are avoided.
    AddToBlockSizeComboBox(DiscIO::WIA_MIN_BLOCK_SIZE);

    break;
  case DiscIO::BlobType::RVZ:
    m_block_size->setEnabled(true);

    for (int block_size = DiscIO::PREFERRED_MIN_BLOCK_SIZE;
         block_size <= DiscIO::PREFERRED_MAX_BLOCK_SIZE; block_size *= 2)
      AddToBlockSizeComboBox(block_size);

    break;
  default:
    break;
  }

  // Populate m_compression
  switch (format)
  {
  case DiscIO::BlobType::GCZ:
    m_compression->setEnabled(true);
    AddToCompressionComboBox(QStringLiteral("Deflate"), DiscIO::WIARVZCompressionType::None);
    break;
  case DiscIO::BlobType::WIA:
  case DiscIO::BlobType::RVZ:
  {
    m_compression->setEnabled(true);

    // i18n: %1 is the name of a compression method (e.g. LZMA)
    const QString slow = tr("%1 (slow)");

    AddToCompressionComboBox(tr("No Compression"), DiscIO::WIARVZCompressionType::None);

    if (format == DiscIO::BlobType::WIA)
      AddToCompressionComboBox(QStringLiteral("Purge"), DiscIO::WIARVZCompressionType::Purge);

    AddToCompressionComboBox(slow.arg(QStringLiteral("bzip2")),
                             DiscIO::WIARVZCompressionType::Bzip2);

    AddToCompressionComboBox(slow.arg(QStringLiteral("LZMA")), DiscIO::WIARVZCompressionType::LZMA);

    AddToCompressionComboBox(slow.arg(QStringLiteral("LZMA2")),
                             DiscIO::WIARVZCompressionType::LZMA2);

    if (format == DiscIO::BlobType::RVZ)
    {
      // i18n: %1 is the name of a compression method (e.g. Zstandard)
      const QString recommended = tr("%1 (recommended)");

      AddToCompressionComboBox(recommended.arg(QStringLiteral("Zstandard")),
                               DiscIO::WIARVZCompressionType::Zstd);
      m_compression->setCurrentIndex(m_compression->count() - 1);
    }

    break;
  }
  default:
    m_compression->setEnabled(false);
    break;
  }

  m_block_size->setEnabled(m_block_size->count() > 1);
  m_compression->setEnabled(m_compression->count() > 1);

  // Block scrubbing of RVZ containers and Datel discs
  const bool scrubbing_allowed =
      format != DiscIO::BlobType::RVZ &&
      std::none_of(m_files.begin(), m_files.end(), std::mem_fn(&UICommon::GameFile::IsDatelDisc));

  m_scrub->setEnabled(scrubbing_allowed);
  if (!scrubbing_allowed)
    m_scrub->setChecked(false);
}

void ConvertDialog::OnCompressionChanged()
{
  m_compression_level->clear();

  const auto compression_type =
      static_cast<DiscIO::WIARVZCompressionType>(m_compression->currentData().toInt());

  const std::pair<int, int> range = DiscIO::GetAllowedCompressionLevels(compression_type, true);

  for (int i = range.first; i <= range.second; ++i)
  {
    AddToCompressionLevelComboBox(i);
    if (i == 5)
      m_compression_level->setCurrentIndex(m_compression_level->count() - 1);
  }

  m_compression_level->setEnabled(m_compression_level->count() > 1);
}

bool ConvertDialog::ShowAreYouSureDialog(const QString& text)
{
  ModalMessageBox warning(this);
  warning.setIcon(QMessageBox::Warning);
  warning.setWindowTitle(tr("Confirm"));
  warning.setText(tr("Are you sure?"));
  warning.setInformativeText(text);
  warning.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

  SetQWidgetWindowDecorations(&warning);
  return warning.exec() == QMessageBox::Yes;
}

void ConvertDialog::Convert()
{
  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(m_format->currentData().toInt());
  const int block_size = m_block_size->currentData().toInt();
  const DiscIO::WIARVZCompressionType compression =
      static_cast<DiscIO::WIARVZCompressionType>(m_compression->currentData().toInt());
  const int compression_level = m_compression_level->currentData().toInt();
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

  if (std::any_of(m_files.begin(), m_files.end(), std::mem_fn(&UICommon::GameFile::IsNKit)))
  {
    if (!ShowAreYouSureDialog(
            tr("Dolphin can't convert NKit files to non-NKit files. Converting an NKit file in "
               "Dolphin will result in another NKit file.\n"
               "\n"
               "If you want to convert an NKit file to a non-NKit file, you can use the same "
               "program as you originally used when converting the file to the NKit format.\n"
               "\n"
               "Do you want to continue anyway?")))
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
    filter = tr("GCZ GC/Wii images (*.gcz)");
    break;
  case DiscIO::BlobType::WIA:
    extension = QStringLiteral(".wia");
    filter = tr("WIA GC/Wii images (*.wia)");
    break;
  case DiscIO::BlobType::RVZ:
    extension = QStringLiteral(".rvz");
    filter = tr("RVZ GC/Wii images (*.rvz)");
    break;
  default:
    ASSERT(false);
    return;
  }

  QString dst_dir;
  QString dst_path;

  if (m_files.size() > 1)
  {
    dst_dir = DolphinFileDialog::getExistingDirectory(
        this, tr("Select where you want to save the converted images"),
        QFileInfo(QString::fromStdString(m_files[0]->GetFilePath())).dir().absolutePath());

    if (dst_dir.isEmpty())
      return;
  }
  else
  {
    dst_path = DolphinFileDialog::getSaveFileName(
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

  int success_count = 0;

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

        SetQWidgetWindowDecorations(&confirm_replace);
        if (confirm_replace.exec() == QMessageBox::No)
          continue;
      }
    }

    if (std::filesystem::exists(StringToPath(dst_path.toStdString())))
    {
      std::error_code ec;
      if (std::filesystem::equivalent(StringToPath(dst_path.toStdString()),
                                      StringToPath(original_path), ec))
      {
        ModalMessageBox::critical(
            this, tr("Error"),
            tr("The destination file cannot be the same as the source file\n\n"
               "Please select another destination path for \"%1\"")
                .arg(QString::fromStdString(original_path)));
        continue;
      }
    }

    ParallelProgressDialog progress_dialog(tr("Converting..."), tr("Abort"), 0, 100, this);
    progress_dialog.GetRaw()->setWindowModality(Qt::WindowModal);
    progress_dialog.GetRaw()->setWindowTitle(tr("Progress"));

    if (m_files.size() > 1)
    {
      // i18n: %1 is a filename.
      progress_dialog.GetRaw()->setLabelText(
          tr("Converting...\n%1").arg(QFileInfo(QString::fromStdString(original_path)).fileName()));
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
      ModalMessageBox::critical(
          this, tr("Error"),
          tr("Failed to open the input file \"%1\".").arg(QString::fromStdString(original_path)));
      return;
    }
    else
    {
      const auto callback = [&progress_dialog](const std::string& text, float percent) {
        progress_dialog.SetValue(percent * 100);
        return !progress_dialog.WasCanceled();
      };

      std::future<bool> success;

      switch (format)
      {
      case DiscIO::BlobType::PLAIN:
        success = std::async(std::launch::async, [&] {
          const bool good = DiscIO::ConvertToPlain(blob_reader.get(), original_path,
                                                   dst_path.toStdString(), callback);
          progress_dialog.Reset();
          return good;
        });
        break;

      case DiscIO::BlobType::GCZ:
        success = std::async(std::launch::async, [&] {
          const bool good = DiscIO::ConvertToGCZ(
              blob_reader.get(), original_path, dst_path.toStdString(),
              file->GetPlatform() == DiscIO::Platform::WiiDisc ? 1 : 0, block_size, callback);
          progress_dialog.Reset();
          return good;
        });
        break;

      case DiscIO::BlobType::WIA:
      case DiscIO::BlobType::RVZ:
        success = std::async(std::launch::async, [&] {
          const bool good =
              DiscIO::ConvertToWIAOrRVZ(blob_reader.get(), original_path, dst_path.toStdString(),
                                        format == DiscIO::BlobType::RVZ, compression,
                                        compression_level, block_size, callback);
          progress_dialog.Reset();
          return good;
        });
        break;

      default:
        ASSERT(false);
        break;
      }

      SetQWidgetWindowDecorations(progress_dialog.GetRaw());
      progress_dialog.GetRaw()->exec();
      if (!success.get())
      {
        ModalMessageBox::critical(this, tr("Error"),
                                  tr("Dolphin failed to complete the requested action."));
        return;
      }

      success_count++;
    }
  }

  ModalMessageBox::information(this, tr("Success"),
                               tr("Successfully converted %n image(s).", "", success_count));

  close();
}
