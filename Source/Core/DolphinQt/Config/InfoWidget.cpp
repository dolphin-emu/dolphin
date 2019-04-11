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

  layout->addRow(tr("Name:"), internal_name);
  layout->addRow(tr("File:"), file_path);
  layout->addRow(tr("Game ID:"), game_id);
  layout->addRow(tr("Country:"), country);
  layout->addRow(tr("Maker:"), maker);

  if (!m_game.GetApploaderDate().empty())
    layout->addRow(tr("Apploader Date:"), CreateValueDisplay(m_game.GetApploaderDate()));

  group->setLayout(layout);
  return group;
}

QGroupBox* InfoWidget::CreateBannerDetails()
{
  QGroupBox* group = new QGroupBox(tr("Banner Details"));
  QFormLayout* layout = new QFormLayout;

  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  CreateLanguageSelector();
  layout->addRow(tr("Show Language:"), m_language_selector);

  if (m_game.GetPlatform() == DiscIO::Platform::GameCubeDisc)
  {
    layout->addRow(tr("Name:"), m_name = CreateValueDisplay());
    layout->addRow(tr("Maker:"), m_maker = CreateValueDisplay());
    layout->addRow(tr("Description:"), m_description = new QTextEdit());
    m_description->setReadOnly(true);
  }
  else if (DiscIO::IsWii(m_game.GetPlatform()))
  {
    layout->addRow(tr("Name:"), m_name = CreateValueDisplay());
  }

  ChangeLanguage();

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
  QLineEdit* value_display = new QLineEdit(value, this);
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
}

void InfoWidget::ChangeLanguage()
{
  DiscIO::Language language =
      static_cast<DiscIO::Language>(m_language_selector->currentData().toInt());

  if (m_name)
    m_name->setText(QString::fromStdString(m_game.GetLongName(language)));

  if (m_maker)
    m_maker->setText(QString::fromStdString(m_game.GetLongMaker(language)));

  if (m_description)
    m_description->setText(QString::fromStdString(m_game.GetDescription(language)));
}
