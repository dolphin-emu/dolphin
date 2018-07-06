// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QTextEdit>

#include "Core/ConfigManager.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"

#include "DolphinQt/Config/InfoWidget.h"
#include "DolphinQt/QtUtils/ImageConverter.h"

#include "UICommon/UICommon.h"

InfoWidget::InfoWidget(const UICommon::GameFile& game) : m_game(game)
{
  QVBoxLayout* layout = new QVBoxLayout();

  layout->addWidget(CreateISODetails());

  if (!game.GetLanguages().empty())
    layout->addWidget(CreateBannerDetails());

  setLayout(layout);
}

QGroupBox* InfoWidget::CreateISODetails()
{
  const QString UNKNOWN_NAME = tr("Unknown");

  QGroupBox* group = new QGroupBox(tr("ISO Details"));
  QFormLayout* layout = new QFormLayout;

  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  QLineEdit* file_path = CreateValueDisplay(
      QStringLiteral("%1 (%2)")
          .arg(QDir::toNativeSeparators(QString::fromStdString(m_game.GetFilePath())))
          .arg(QString::fromStdString(UICommon::FormatSize(m_game.GetFileSize()))));

  const QString game_name = QString::fromStdString(m_game.GetInternalName());

  bool is_disc_based = m_game.GetPlatform() == DiscIO::Platform::GameCubeDisc ||
                       m_game.GetPlatform() == DiscIO::Platform::WiiDisc;

  QLineEdit* internal_name =
      CreateValueDisplay(is_disc_based ? tr("%1 (Disc %2, Revision %3)")
                                             .arg(game_name.isEmpty() ? UNKNOWN_NAME : game_name)
                                             .arg(m_game.GetDiscNumber())
                                             .arg(m_game.GetRevision()) :
                                         tr("%1 (Revision %3)")
                                             .arg(game_name.isEmpty() ? UNKNOWN_NAME : game_name)
                                             .arg(m_game.GetRevision()));

  QString game_id_string = QString::fromStdString(m_game.GetGameID());

  if (const u64 title_id = m_game.GetTitleID())
    game_id_string += QStringLiteral(" (%1)").arg(title_id, 16, 16, QLatin1Char('0'));

  QLineEdit* game_id = CreateValueDisplay(game_id_string);

  QLineEdit* country = CreateValueDisplay(DiscIO::GetName(m_game.GetCountry(), true));

  const std::string game_maker = m_game.GetMaker();

  QLineEdit* maker =
      CreateValueDisplay((game_maker.empty() ? UNKNOWN_NAME.toStdString() : game_maker) + " (" +
                         m_game.GetMakerID() + ")");
  QWidget* checksum = CreateChecksumComputer();

  layout->addRow(tr("Name:"), internal_name);
  layout->addRow(tr("File:"), file_path);
  layout->addRow(tr("Game ID:"), game_id);
  layout->addRow(tr("Country:"), country);
  layout->addRow(tr("Maker:"), maker);

  if (!m_game.GetApploaderDate().empty())
    layout->addRow(tr("Apploader Date:"), CreateValueDisplay(m_game.GetApploaderDate()));

  layout->addRow(tr("MD5 Checksum:"), checksum);

  group->setLayout(layout);
  return group;
}

QGroupBox* InfoWidget::CreateBannerDetails()
{
  QGroupBox* group = new QGroupBox(tr("Banner Details"));
  QFormLayout* layout = new QFormLayout;

  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

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
  const bool is_wii = DiscIO::IsWii(m_game.GetPlatform());
  const DiscIO::Language preferred_language = SConfig::GetInstance().GetCurrentLanguage(is_wii);

  m_language_selector = new QComboBox();
  for (DiscIO::Language language : m_game.GetLanguages())
  {
    m_language_selector->addItem(QString::fromStdString(DiscIO::GetName(language, true)),
                                 static_cast<int>(language));
    if (language == preferred_language)
      m_language_selector->setCurrentIndex(m_language_selector->count() - 1);
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
  progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowContextHelpButtonHint);
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
