// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QComboBox>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QTextEdit>

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DolphinQt2/Config/InfoWidget.h"
#include "DolphinQt2/QtUtils/ImageConverter.h"
#include "UICommon/UICommon.h"

InfoWidget::InfoWidget(const UICommon::GameFile& game) : m_game(game)
{
  QVBoxLayout* layout = new QVBoxLayout();

  layout->addWidget(CreateISODetails());
  layout->addWidget(CreateBannerDetails());

  setLayout(layout);
}

QGroupBox* InfoWidget::CreateISODetails()
{
  QGroupBox* group = new QGroupBox(tr("ISO Details"));
  QFormLayout* layout = new QFormLayout;

  QLineEdit* file_path = CreateValueDisplay(
      QStringLiteral("%1 (%2)")
          .arg(QString::fromStdString(m_game.GetFilePath()))
          .arg(QString::fromStdString(UICommon::FormatSize(m_game.GetFileSize()))));
  QLineEdit* internal_name =
      CreateValueDisplay(tr("%1 (Disc %2, Revision %3)")
                             .arg(QString::fromStdString(m_game.GetInternalName()))
                             .arg(m_game.GetDiscNumber())
                             .arg(m_game.GetRevision()));

  QString game_id_string = QString::fromStdString(m_game.GetGameID());
  if (const u64 title_id = m_game.GetTitleID())
    game_id_string += QStringLiteral(" (%1)").arg(title_id, 16, 16, QLatin1Char('0'));
  QLineEdit* game_id = CreateValueDisplay(game_id_string);

  QLineEdit* country = CreateValueDisplay(DiscIO::GetName(m_game.GetCountry(), true));
  QLineEdit* maker = CreateValueDisplay(m_game.GetMaker() + " (0x" + m_game.GetMakerID() + ")");
  QLineEdit* apploader_date = CreateValueDisplay(m_game.GetApploaderDate());
  QWidget* checksum = CreateChecksumComputer();

  layout->addRow(tr("Name:"), internal_name);
  layout->addRow(tr("File:"), file_path);
  layout->addRow(tr("Game ID:"), game_id);
  layout->addRow(tr("Country:"), country);
  layout->addRow(tr("Maker:"), maker);
  layout->addRow(tr("Apploader Date:"), apploader_date);
  layout->addRow(tr("MD5 Checksum:"), checksum);

  group->setLayout(layout);
  return group;
}

QGroupBox* InfoWidget::CreateBannerDetails()
{
  QGroupBox* group = new QGroupBox(tr("Banner Details"));
  QFormLayout* layout = new QFormLayout;

  m_name = CreateValueDisplay();
  m_maker = CreateValueDisplay();
  m_description = new QTextEdit();
  m_description->setReadOnly(true);
  CreateLanguageSelector();

  layout->addRow(tr("Show Language:"), m_language_selector);
  if (m_game.GetPlatform() == DiscIO::Platform::GameCubeDisc)
  {
    layout->addRow(tr("Name:"), m_name);
    layout->addRow(tr("Maker:"), m_maker);
    layout->addRow(tr("Description:"), m_description);
  }
  else if (DiscIO::IsWii(m_game.GetPlatform()))
  {
    layout->addRow(tr("Name:"), m_name);
  }

  QPixmap banner = ToQPixmap(m_game.GetBannerImage());
  if (!banner.isNull())
    layout->addRow(tr("Banner:"), CreateBannerGraphic(banner));

  group->setLayout(layout);
  return group;
}

QWidget* InfoWidget::CreateBannerGraphic(const QPixmap& image)
{
  QWidget* widget = new QWidget();
  QHBoxLayout* layout = new QHBoxLayout();

  QLabel* banner = new QLabel();
  banner->setPixmap(image);
  QPushButton* save = new QPushButton(tr("Save as..."));
  connect(save, &QPushButton::clicked, this, &InfoWidget::SaveBanner);

  layout->addWidget(banner);
  layout->addWidget(save);
  widget->setLayout(layout);
  return widget;
}

void InfoWidget::SaveBanner()
{
  QString path = QFileDialog::getSaveFileName(this, tr("Select a File"), QDir::currentPath(),
                                              tr("PNG image file (*.png);; All Files (*)"));
  ToQPixmap(m_game.GetBannerImage()).save(path, "PNG");
}

QLineEdit* InfoWidget::CreateValueDisplay(const QString& value)
{
  QLineEdit* value_display = new QLineEdit(value);
  value_display->setReadOnly(true);
  value_display->setCursorPosition(0);
  return value_display;
}

QLineEdit* InfoWidget::CreateValueDisplay(const std::string& value)
{
  return CreateValueDisplay(QString::fromStdString(value));
}

void InfoWidget::CreateLanguageSelector()
{
  m_language_selector = new QComboBox();
  for (DiscIO::Language language : m_game.GetLanguages())
  {
    m_language_selector->addItem(QString::fromStdString(DiscIO::GetName(language, true)),
                                 static_cast<int>(language));
  }
  if (m_language_selector->count() == 1)
    m_language_selector->setDisabled(true);
  connect(m_language_selector,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &InfoWidget::ChangeLanguage);
  ChangeLanguage();
}

void InfoWidget::ChangeLanguage()
{
  DiscIO::Language language =
      static_cast<DiscIO::Language>(m_language_selector->currentData().toInt());
  m_name->setText(QString::fromStdString(m_game.GetLongName(language)));
  m_maker->setText(QString::fromStdString(m_game.GetLongMaker(language)));
  m_description->setText(QString::fromStdString(m_game.GetDescription(language)));
}

QWidget* InfoWidget::CreateChecksumComputer()
{
  QWidget* widget = new QWidget();
  QHBoxLayout* layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  m_checksum_result = new QLineEdit();
  QPushButton* calculate = new QPushButton(tr("Compute"));
  connect(calculate, &QPushButton::clicked, this, &InfoWidget::ComputeChecksum);
  layout->addWidget(m_checksum_result);
  layout->addWidget(calculate);

  widget->setLayout(layout);
  return widget;
}

void InfoWidget::ComputeChecksum()
{
  QCryptographicHash hash(QCryptographicHash::Md5);
  hash.reset();
  std::unique_ptr<DiscIO::BlobReader> file(DiscIO::CreateBlobReader(m_game.GetFilePath()));
  std::vector<u8> file_data(8 * 1080 * 1080);  // read 1MB at a time
  u64 game_size = file->GetDataSize();
  u64 read_offset = 0;

  // a maximum of 1000 is used instead of game_size because otherwise 8GB games overflow the int
  // typed maximum parameter
  QProgressDialog* progress =
      new QProgressDialog(tr("Computing MD5 Checksum"), tr("Cancel"), 0, 1000, this);
  progress->setWindowTitle(tr("Computing MD5 Checksum"));
  progress->setMinimumDuration(500);
  progress->setWindowModality(Qt::WindowModal);
  while (read_offset < game_size)
  {
    progress->setValue(static_cast<double>(read_offset) / static_cast<double>(game_size) * 1000);
    if (progress->wasCanceled())
      return;

    u64 read_size = std::min<u64>(file_data.size(), game_size - read_offset);
    file->Read(read_offset, read_size, file_data.data());
    hash.addData(reinterpret_cast<char*>(file_data.data()), read_size);
    read_offset += read_size;
  }
  m_checksum_result->setText(QString::fromUtf8(hash.result().toHex()));
  Q_ASSERT(read_offset == game_size);
  progress->setValue(1000);
}
