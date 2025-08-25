// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/PropertiesDialog.h"

#include <memory>

#include <QDialogButtonBox>
#include <QPushButton>
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

PropertiesDialog::PropertiesDialog(QWidget* parent, const UICommon::GameFile& game)
    : TabbedSettingsWindow{parent}, m_filepath(game.GetFilePath())
{
  setWindowTitle(QStringLiteral("%1: %2 - %3")
                     .arg(QString::fromStdString(game.GetFileName()),
                          QString::fromStdString(game.GetGameID()),
                          QString::fromStdString(game.GetLongName())));

  auto* const info = new InfoWidget(game);
  auto* const ar = new ARCodeWidget(game.GetGameID(), game.GetRevision());
  auto* const gecko =
      new GeckoCodeWidget(game.GetGameID(), game.GetGameTDBID(), game.GetRevision());
  auto* const patches = new PatchesWidget(game);
  auto* const game_config = new GameConfigWidget(game);
  auto* const graphics_mod_list = new GraphicsModListWidget(game);

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

  // Note: Intentional selective use of AddWrappedPane for a sensible dialog "minimumSize".
  AddWrappedPane(info, tr("Info"));
  AddPane(game_config, tr("Game Config"));
  AddPane(patches, tr("Patches"));
  AddPane(ar, tr("AR Codes"));
  AddPane(gecko, tr("Gecko Codes"));
  AddWrappedPane(graphics_mod_list, tr("Graphics Mods"));

  if (game.GetPlatform() != DiscIO::Platform::ELFOrDOL)
  {
    std::shared_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(game.GetFilePath());
    if (volume)
    {
      auto* const verify = new VerifyWidget(volume);
      AddPane(verify, tr("Verify"));

      if (DiscIO::IsDisc(game.GetPlatform()))
      {
        auto* const filesystem = new FilesystemWidget(volume);
        AddPane(filesystem, tr("Filesystem"));
      }
    }
  }

  connect(this, &QDialog::rejected, graphics_mod_list, &GraphicsModListWidget::SaveToDisk);

  OnDoneCreatingPanes();
}
