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

InfoWidget::InfoWidget(const GameFile& game) : m_game(game)
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

  QLineEdit* file_path = CreateValueDisplay(m_game.GetFilePath());
  QLineEdit* internal_name = CreateValueDisplay(m_game.GetInternalName());
  QLineEdit* game_id = CreateValueDisplay(m_game.GetGameID());
  QLineEdit* country = CreateValueDisplay(m_game.GetCountry());
  QLineEdit* maker = CreateValueDisplay(m_game.GetMaker());
  QLineEdit* maker_id = CreateValueDisplay(QStringLiteral("0x") + m_game.GetMakerID());
  QLineEdit* disc_number = CreateValueDisplay(QString::number(m_game.GetDiscNumber()));
  QLineEdit* revision = CreateValueDisplay(QString::number(m_game.GetRevision()));
  QLineEdit* apploader_date = CreateValueDisplay(m_game.GetApploaderDate());
  QLineEdit* iso_size = CreateValueDisplay(FormatSize(m_game.GetFileSize()));
  QWidget* checksum = CreateChecksumComputer();

  layout->addRow(tr("File Path:"), file_path);
  layout->addRow(tr("Internal Name:"), internal_name);
  layout->addRow(tr("Game ID:"), game_id);
  layout->addRow(tr("Country:"), country);
  layout->addRow(tr("Maker:"), maker);
  layout->addRow(tr("Maker ID:"), maker_id);
  layout->addRow(tr("Disc Number:"), disc_number);
  layout->addRow(tr("Revision:"), revision);
  layout->addRow(tr("Apploader Date:"), apploader_date);
  layout->addRow(tr("ISO Size:"), iso_size);
  layout->addRow(tr("MD5 Checksum:"), checksum);

  group->setLayout(layout);
  return group;
}

QGroupBox* InfoWidget::CreateBannerDetails()
{
  QGroupBox* group = new QGroupBox(tr("Banner Details"));
  QFormLayout* layout = new QFormLayout;

  m_long_name = CreateValueDisplay();
  m_short_name = CreateValueDisplay();
  m_short_maker = CreateValueDisplay();
  m_long_maker = CreateValueDisplay();
  m_description = new QTextEdit();
  m_description->setReadOnly(true);
  CreateLanguageSelector();

  layout->addRow(tr("Show Language:"), m_language_selector);
  if (m_game.GetPlatformID() == DiscIO::Platform::GAMECUBE_DISC)
  {
    layout->addRow(tr("Short Name:"), m_short_name);
    layout->addRow(tr("Short Maker:"), m_short_maker);
    layout->addRow(tr("Long Name:"), m_long_name);
    layout->addRow(tr("Long Maker:"), m_long_maker);
    layout->addRow(tr("Description:"), m_description);
  }
  else if (DiscIO::IsWii(m_game.GetPlatformID()))
  {
    layout->addRow(tr("Name:"), m_long_name);
  }

  if (!m_game.GetBanner().isNull())
  {
    layout->addRow(tr("Banner:"), CreateBannerGraphic());
  }

  group->setLayout(layout);
  return group;
}

QWidget* InfoWidget::CreateBannerGraphic()
{
  QWidget* widget = new QWidget();
  QHBoxLayout* layout = new QHBoxLayout();

  QLabel* banner = new QLabel();
  banner->setPixmap(m_game.GetBanner());
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
  m_game.GetBanner().save(path, "PNG");
}

QLineEdit* InfoWidget::CreateValueDisplay(const QString& value)
{
  QLineEdit* value_display = new QLineEdit(value);
  value_display->setReadOnly(true);
  value_display->setCursorPosition(0);
  return value_display;
}

void InfoWidget::CreateLanguageSelector()
{
  m_language_selector = new QComboBox();
  QList<DiscIO::Language> languages = m_game.GetAvailableLanguages();
  for (int i = 0; i < languages.count(); i++)
  {
    DiscIO::Language language = languages.at(i);
    m_language_selector->addItem(m_game.GetLanguage(language), static_cast<int>(language));
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
  m_short_name->setText(m_game.GetShortName(language));
  m_short_maker->setText(m_game.GetShortMaker(language));
  m_long_name->setText(m_game.GetLongName(language));
  m_long_maker->setText(m_game.GetLongMaker(language));
  m_description->setText(m_game.GetDescription(language));
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
  std::unique_ptr<DiscIO::BlobReader> file(
      DiscIO::CreateBlobReader(m_game.GetFilePath().toStdString()));
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
