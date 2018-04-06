// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DolphinQt2/Config/ARCodeWidget.h"
#include "DolphinQt2/Config/FilesystemWidget.h"
#include "DolphinQt2/Config/GameConfigWidget.h"
#include "DolphinQt2/Config/GeckoCodeWidget.h"
#include "DolphinQt2/Config/InfoWidget.h"
#include "DolphinQt2/Config/PatchesWidget.h"
#include "DolphinQt2/Config/PropertiesDialog.h"
#include "DolphinQt2/QtUtils/WrapInScrollArea.h"
#include "UICommon/GameFile.h"

PropertiesDialog::PropertiesDialog(QWidget* parent, const UICommon::GameFile& game)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("%1: %2 - %3")
                     .arg(QString::fromStdString(game.GetFileName()),
                          QString::fromStdString(game.GetGameID()),
                          QString::fromStdString(game.GetLongName())));
  QVBoxLayout* layout = new QVBoxLayout();

  QTabWidget* tab_widget = new QTabWidget(this);
  InfoWidget* info = new InfoWidget(game);

  ARCodeWidget* ar = new ARCodeWidget(game);
  GeckoCodeWidget* gecko = new GeckoCodeWidget(game);
  PatchesWidget* patches = new PatchesWidget(game);
  GameConfigWidget* game_config = new GameConfigWidget(game);

  connect(gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &PropertiesDialog::OpenGeneralSettings);

  connect(ar, &ARCodeWidget::OpenGeneralSettings, this, &PropertiesDialog::OpenGeneralSettings);

  tab_widget->addTab(GetWrappedWidget(game_config, this, 50, 80), tr("Game Config"));
  tab_widget->addTab(GetWrappedWidget(patches, this, 50, 80), tr("Patches"));
  tab_widget->addTab(GetWrappedWidget(ar, this, 50, 80), tr("AR Codes"));
  tab_widget->addTab(GetWrappedWidget(gecko, this, 50, 80), tr("Gecko Codes"));
  tab_widget->addTab(GetWrappedWidget(info, this, 50, 80), tr("Info"));

  if (DiscIO::IsDisc(game.GetPlatform()))
  {
    FilesystemWidget* filesystem = new FilesystemWidget(game);
    tab_widget->addTab(GetWrappedWidget(filesystem, this, 50, 80), tr("Filesystem"));
  }

  layout->addWidget(tab_widget);

  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(close_box);

  setLayout(layout);
}
