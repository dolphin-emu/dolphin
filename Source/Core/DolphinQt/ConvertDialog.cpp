// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/ConvertDialog.h"

#include <algorithm>
#include <future>
#include <memory>
#include <utility>

#include <QComboBox>
#include <QErrorMessage>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "Common/Assert.h"
#include "DiscIO/Blob.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "UICommon/GameFile.h"

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

  QPushButton* convert_button = new QPushButton(tr("Convert"));

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addLayout(grid_layout);
  main_layout->addWidget(convert_button);

  setLayout(main_layout);

  connect(convert_button, &QPushButton::clicked, this, &ConvertDialog::Convert);
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

void ConvertDialog::Convert()
{
  const DiscIO::BlobType format = static_cast<DiscIO::BlobType>(m_format->currentData().toInt());

  const bool scrub_wii = format == DiscIO::BlobType::GCZ;

  if (scrub_wii && std::any_of(m_files.begin(), m_files.end(), [](const auto& file) {
        return file->GetPlatform() == DiscIO::Platform::WiiDisc;
      }))
  {
    ModalMessageBox wii_warning(this);
    wii_warning.setIcon(QMessageBox::Warning);
    wii_warning.setWindowTitle(tr("Confirm"));
    wii_warning.setText(tr("Are you sure?"));
    wii_warning.setInformativeText(
        tr("Compressing a Wii disc image will irreversibly change the compressed copy by removing "
           "padding data. Your disc image will still work. Continue?"));
    wii_warning.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    if (wii_warning.exec() == QMessageBox::No)
      return;
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
    bool scrub_current_file = scrub_wii && file->GetPlatform() == DiscIO::Platform::WiiDisc;

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
                                   file->GetPlatform() == DiscIO::Platform::WiiDisc ? 1 : 0, 16384,
                                   &CompressCB, &progress_dialog);
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
