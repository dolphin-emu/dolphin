// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/PropertiesDialog.h"

#include <memory>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/FilesystemWidget.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/Config/GraphicsModListWidget.h"
#include "DolphinQt/Config/InfoWidget.h"
#include "DolphinQt/Config/PatchesWidget.h"
#include "DolphinQt/Config/VerifyWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "UICommon/GameFile.h"
#include "VideoCommon/VideoConfig.h"

PropertiesDialog::PropertiesDialog(QWidget* parent, const UICommon::GameFile& game)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("%1: %2 - %3")
                     .arg(QString::fromStdString(game.GetFileName()),
                          QString::fromStdString(game.GetGameID()),
                          QString::fromStdString(game.GetLongName())));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QVBoxLayout* layout = new QVBoxLayout();

  QTabWidget* tab_widget = new QTabWidget(this);
  InfoWidget* info = new InfoWidget(game);

  ARCodeWidget* ar = new ARCodeWidget(game.GetGameID(), game.GetRevision());
  GeckoCodeWidget* gecko =
      new GeckoCodeWidget(game.GetGameID(), game.GetGameTDBID(), game.GetRevision());
  PatchesWidget* patches = new PatchesWidget(game);
  GameConfigWidget* game_config = new GameConfigWidget(game);
  GraphicsModListWidget* graphics_mod_list = new GraphicsModListWidget(game);

  connect(gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &PropertiesDialog::OpenGeneralSettings);

  connect(ar, &ARCodeWidget::OpenGeneralSettings, this, &PropertiesDialog::OpenGeneralSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(ar, &ARCodeWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
  connect(gecko, &GeckoCodeWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
  connect(patches, &PatchesWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  connect(graphics_mod_list, &GraphicsModListWidget::OpenGraphicsSettings, this,
          &PropertiesDialog::OpenGraphicsSettings);

  const int padding_width = 600;
  const int padding_height = 1100;
  tab_widget->addTab(GetWrappedWidget(game_config, this, padding_width, padding_height),
                     tr("Game Config"));
  tab_widget->addTab(GetWrappedWidget(patches, this, padding_width, padding_height), tr("Patches"));
  tab_widget->addTab(GetWrappedWidget(ar, this, padding_width, padding_height), tr("AR Codes"));
  tab_widget->addTab(GetWrappedWidget(gecko, this, padding_width, padding_height),
                     tr("Gecko Codes"));
  tab_widget->addTab(GetWrappedWidget(graphics_mod_list, this, padding_width, padding_height),
                     tr("Graphics Mods"));
  tab_widget->addTab(GetWrappedWidget(info, this, padding_width, padding_height), tr("Info"));

  if (game.GetPlatform() != DiscIO::Platform::ELFOrDOL)
  {
    std::shared_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(game.GetFilePath());
    if (volume)
    {
      VerifyWidget* verify = new VerifyWidget(volume);
      tab_widget->addTab(GetWrappedWidget(verify, this, padding_width, padding_height),
                         tr("Verify"));

      if (DiscIO::IsDisc(game.GetPlatform()))
      {
        FilesystemWidget* filesystem = new FilesystemWidget(volume);
        tab_widget->addTab(GetWrappedWidget(filesystem, this, padding_width, padding_height),
                           tr("Filesystem"));
      }
    }
  }

  layout->addWidget(tab_widget);

  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(close_box, &QDialogButtonBox::rejected, graphics_mod_list,
          &GraphicsModListWidget::SaveToDisk);

  layout->addWidget(close_box);

  setLayout(layout);
  tab_widget->setCurrentIndex(3);
}

GeckoDialog::GeckoDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(QStringLiteral("%1").arg(QString::fromStdString("Modifications")));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QVBoxLayout* layout = new QVBoxLayout();
  QTabWidget* tab_widget = new QTabWidget(this);
  GeckoCodeWidget* mp4_gecko = new GeckoCodeWidget("GMPE01", "GMPE01", 0);
  GeckoCodeWidget* mp5_gecko = new GeckoCodeWidget("GP5E01", "GP5E01", 0);
  GeckoCodeWidget* mp6_gecko = new GeckoCodeWidget("GP6E01", "GP6E01", 0);
  GeckoCodeWidget* mp7_gecko = new GeckoCodeWidget("GP7E01", "GP7E01", 0);
  GeckoCodeWidget* mp8_gecko = new GeckoCodeWidget("RM8E01", "RM8E01", 0);

  connect(mp4_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp5_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp6_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp7_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp8_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);

  const int padding_width = 600;
  const int padding_height = 1100;

  tab_widget->addTab(GetWrappedWidget(mp4_gecko, this, padding_width, padding_height),
                     tr("Mario Party 4"));
  tab_widget->addTab(GetWrappedWidget(mp5_gecko, this, padding_width, padding_height),
                     tr("Mario Party 5"));
  tab_widget->addTab(GetWrappedWidget(mp6_gecko, this, padding_width, padding_height),
                     tr("Mario Party 6"));
  tab_widget->addTab(GetWrappedWidget(mp7_gecko, this, padding_width, padding_height),
                     tr("Mario Party 7"));
  tab_widget->addTab(GetWrappedWidget(mp8_gecko, this, padding_width, padding_height),
                     tr("Mario Party 8"));

  layout->addWidget(tab_widget);

  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(close_box);
  setLayout(layout);
}
