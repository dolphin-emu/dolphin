// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/MenuBar.h"

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMap>
#include <QMessageBox>
#include <QUrl>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/State.h"
#include "DiscIO/NANDImporter.h"
#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/Settings.h"

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
  AddFileMenu();
  AddEmulationMenu();
  addMenu(tr("Movie"));
  AddOptionsMenu();
  AddToolsMenu();
  AddViewMenu();
  AddHelpMenu();

  EmulationStopped();
}

void MenuBar::EmulationStarted()
{
  // Emulation
  m_play_action->setEnabled(false);
  m_play_action->setVisible(false);
  m_pause_action->setEnabled(true);
  m_pause_action->setVisible(true);
  m_stop_action->setEnabled(true);
  m_reset_action->setEnabled(true);
  m_fullscreen_action->setEnabled(true);
  m_frame_advance_action->setEnabled(true);
  m_screenshot_action->setEnabled(true);
  m_state_load_menu->setEnabled(true);
  m_state_save_menu->setEnabled(true);
  UpdateStateSlotMenu();
  UpdateToolsMenu(true);
}
void MenuBar::EmulationPaused()
{
  m_play_action->setEnabled(true);
  m_play_action->setVisible(true);
  m_pause_action->setEnabled(false);
  m_pause_action->setVisible(false);
}
void MenuBar::EmulationStopped()
{
  // Emulation
  m_play_action->setEnabled(true);
  m_play_action->setVisible(true);
  m_pause_action->setEnabled(false);
  m_pause_action->setVisible(false);
  m_stop_action->setEnabled(false);
  m_reset_action->setEnabled(false);
  m_fullscreen_action->setEnabled(false);
  m_frame_advance_action->setEnabled(false);
  m_screenshot_action->setEnabled(false);
  m_state_load_menu->setEnabled(false);
  m_state_save_menu->setEnabled(false);
  UpdateStateSlotMenu();
  UpdateToolsMenu(false);
}

void MenuBar::AddFileMenu()
{
  QMenu* file_menu = addMenu(tr("&File"));
  m_open_action = file_menu->addAction(tr("&Open..."), this, &MenuBar::Open,
                                       QKeySequence(QStringLiteral("Ctrl+O")));
  m_exit_action = file_menu->addAction(tr("E&xit"), this, &MenuBar::Exit,
                                       QKeySequence(QStringLiteral("Alt+F4")));
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("&Tools"));

  tools_menu->addAction(tr("Import Wii Save..."), this, &MenuBar::ImportWiiSave);
  tools_menu->addAction(tr("Export All Wii Saves"), this, &MenuBar::ExportWiiSaves);

  tools_menu->addSeparator();

  m_wad_install_action = tools_menu->addAction(tr("Install WAD..."), this, &MenuBar::InstallWAD);

  tools_menu->addSeparator();
  QMenu* gc_ipl = tools_menu->addMenu(tr("Load GameCube Main Menu"));

  m_ntscj_ipl = gc_ipl->addAction(tr("NTSC-J"), this,
                                  [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_J); });
  m_ntscu_ipl = gc_ipl->addAction(tr("NTSC-U"), this,
                                  [this] { emit BootGameCubeIPL(DiscIO::Region::NTSC_U); });
  m_pal_ipl =
      gc_ipl->addAction(tr("PAL"), this, [this] { emit BootGameCubeIPL(DiscIO::Region::PAL); });

  tools_menu->addAction(tr("Start &NetPlay..."), this, &MenuBar::StartNetPlay);
  tools_menu->addSeparator();

  // Label will be set by a NANDRefresh later
  m_boot_sysmenu = tools_menu->addAction(QStringLiteral(""), [this] { emit BootWiiSystemMenu(); });
  m_import_backup = tools_menu->addAction(tr("Import BootMii NAND Backup..."),
                                          [this] { emit ImportNANDBackup(); });

  m_extract_certificates = tools_menu->addAction(tr("Extract Certificates from NAND"), this,
                                                 &MenuBar::NANDExtractCertificates);

  m_boot_sysmenu->setEnabled(false);

  connect(&Settings::Instance(), &Settings::NANDRefresh, [this] { UpdateToolsMenu(false); });

  m_perform_online_update_menu = tools_menu->addMenu(tr("Perform Online System Update"));
  m_perform_online_update_for_current_region = m_perform_online_update_menu->addAction(
      tr("Current Region"), [this] { emit PerformOnlineUpdate(""); });
  m_perform_online_update_menu->addSeparator();
  m_perform_online_update_menu->addAction(tr("Europe"),
                                          [this] { emit PerformOnlineUpdate("EUR"); });
  m_perform_online_update_menu->addAction(tr("Japan"), [this] { emit PerformOnlineUpdate("JPN"); });
  m_perform_online_update_menu->addAction(tr("Korea"), [this] { emit PerformOnlineUpdate("KOR"); });
  m_perform_online_update_menu->addAction(tr("United States"),
                                          [this] { emit PerformOnlineUpdate("USA"); });
}

void MenuBar::AddEmulationMenu()
{
  QMenu* emu_menu = addMenu(tr("&Emulation"));
  m_play_action = emu_menu->addAction(tr("&Play"), this, &MenuBar::Play);
  m_pause_action = emu_menu->addAction(tr("&Pause"), this, &MenuBar::Pause);
  m_stop_action = emu_menu->addAction(tr("&Stop"), this, &MenuBar::Stop);
  m_reset_action = emu_menu->addAction(tr("&Reset"), this, &MenuBar::Reset);
  m_fullscreen_action = emu_menu->addAction(tr("Toggle &Fullscreen"), this, &MenuBar::Fullscreen);
  m_frame_advance_action = emu_menu->addAction(tr("&Frame Advance"), this, &MenuBar::FrameAdvance);

  m_screenshot_action = emu_menu->addAction(tr("Take Screenshot"), this, &MenuBar::Screenshot);

  emu_menu->addSeparator();

  AddStateLoadMenu(emu_menu);
  AddStateSaveMenu(emu_menu);
  AddStateSlotMenu(emu_menu);
  UpdateStateSlotMenu();
}

void MenuBar::AddStateLoadMenu(QMenu* emu_menu)
{
  m_state_load_menu = emu_menu->addMenu(tr("&Load State"));
  m_state_load_menu->addAction(tr("Load State from File"), this, &MenuBar::StateLoad);
  m_state_load_menu->addAction(tr("Load State from Selected Slot"), this, &MenuBar::StateLoadSlot);
  m_state_load_slots_menu = m_state_load_menu->addMenu(tr("Load State from Slot"));
  m_state_load_menu->addAction(tr("Undo Load State"), this, &MenuBar::StateLoadUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_load_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateLoadSlotAt(i); });
  }
}

void MenuBar::AddStateSaveMenu(QMenu* emu_menu)
{
  m_state_save_menu = emu_menu->addMenu(tr("Sa&ve State"));
  m_state_save_menu->addAction(tr("Save State to File"), this, &MenuBar::StateSave);
  m_state_save_menu->addAction(tr("Save State to Selected Slot"), this, &MenuBar::StateSaveSlot);
  m_state_save_menu->addAction(tr("Save State to Oldest Slot"), this, &MenuBar::StateSaveOldest);
  m_state_save_slots_menu = m_state_save_menu->addMenu(tr("Save State to Slot"));
  m_state_save_menu->addAction(tr("Undo Save State"), this, &MenuBar::StateSaveUndo);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_save_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateSaveSlotAt(i); });
  }
}

void MenuBar::AddStateSlotMenu(QMenu* emu_menu)
{
  m_state_slot_menu = emu_menu->addMenu(tr("Select State Slot"));
  m_state_slots = new QActionGroup(this);

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_slot_menu->addAction(QStringLiteral(""));
    action->setCheckable(true);
    action->setActionGroup(m_state_slots);
    if (Settings::Instance().GetStateSlot() == i)
      action->setChecked(true);

    connect(action, &QAction::triggered, this, [=]() { emit SetStateSlot(i); });
  }
}

void MenuBar::UpdateStateSlotMenu()
{
  QList<QAction*> actions_slot = m_state_slots->actions();
  QList<QAction*> actions_load = m_state_load_slots_menu->actions();
  QList<QAction*> actions_save = m_state_save_slots_menu->actions();
  for (int i = 0; i < actions_slot.length(); i++)
  {
    int slot = i + 1;
    QString info = QString::fromStdString(State::GetInfoStringOfSlot(slot));
    actions_load.at(i)->setText(tr("Load from Slot %1 - %2").arg(slot).arg(info));
    actions_save.at(i)->setText(tr("Save to Slot %1 - %2").arg(slot).arg(info));
    actions_slot.at(i)->setText(tr("Select Slot %1 - %2").arg(slot).arg(info));
  }
}

void MenuBar::AddViewMenu()
{
  QMenu* view_menu = addMenu(tr("&View"));
  QAction* show_log = view_menu->addAction(tr("Show &Log"));
  show_log->setCheckable(true);
  show_log->setChecked(Settings::Instance().IsLogVisible());

  connect(show_log, &QAction::toggled, &Settings::Instance(), &Settings::SetLogVisible);

  QAction* show_log_config = view_menu->addAction(tr("Show Log &Configuration"));
  show_log_config->setCheckable(true);
  show_log_config->setChecked(Settings::Instance().IsLogConfigVisible());

  connect(show_log_config, &QAction::toggled, &Settings::Instance(),
          &Settings::SetLogConfigVisible);

  connect(&Settings::Instance(), &Settings::LogVisibilityChanged, show_log, &QAction::setChecked);
  connect(&Settings::Instance(), &Settings::LogConfigVisibilityChanged, show_log_config,
          &QAction::setChecked);

  view_menu->addSeparator();

  AddGameListTypeSection(view_menu);
  view_menu->addSeparator();
  AddListColumnsMenu(view_menu);
  view_menu->addSeparator();
  AddShowPlatformsMenu(view_menu);
  AddShowRegionsMenu(view_menu);
}

void MenuBar::AddOptionsMenu()
{
  QMenu* options_menu = addMenu(tr("&Options"));
  options_menu->addAction(tr("Co&nfiguration"), this, &MenuBar::Configure);
  options_menu->addSeparator();
  options_menu->addAction(tr("&Graphics Settings"), this, &MenuBar::ConfigureGraphics);
  options_menu->addAction(tr("&Audio Settings"), this, &MenuBar::ConfigureAudio);
  options_menu->addAction(tr("&Controller Settings"), this, &MenuBar::ConfigureControllers);
  options_menu->addAction(tr("&Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);
}

void MenuBar::AddHelpMenu()
{
  QMenu* help_menu = addMenu(tr("&Help"));
  QAction* website = help_menu->addAction(tr("&Website"));
  connect(website, &QAction::triggered, this,
          []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/"))); });
  QAction* documentation = help_menu->addAction(tr("Online &Documentation"));
  connect(documentation, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
  });
  QAction* github = help_menu->addAction(tr("&GitHub Repository"));
  connect(github, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/dolphin-emu/dolphin")));
  });

  help_menu->addSeparator();
  help_menu->addAction(tr("&About"), this, &MenuBar::ShowAboutDialog);
}

void MenuBar::AddGameListTypeSection(QMenu* view_menu)
{
  QAction* list_view = view_menu->addAction(tr("List View"));
  list_view->setCheckable(true);

  QAction* grid_view = view_menu->addAction(tr("Grid View"));
  grid_view->setCheckable(true);

  QActionGroup* list_group = new QActionGroup(this);
  list_group->addAction(list_view);
  list_group->addAction(grid_view);

  bool prefer_list = Settings::Instance().GetPreferredView();
  list_view->setChecked(prefer_list);
  grid_view->setChecked(!prefer_list);

  connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
  connect(grid_view, &QAction::triggered, this, &MenuBar::ShowGrid);
}

void MenuBar::AddListColumnsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> columns{
      {tr("Platform"), &SConfig::GetInstance().m_showSystemColumn},
      {tr("ID"), &SConfig::GetInstance().m_showIDColumn},
      {tr("Banner"), &SConfig::GetInstance().m_showBannerColumn},
      {tr("Title"), &SConfig::GetInstance().m_showTitleColumn},
      {tr("Description"), &SConfig::GetInstance().m_showDescriptionColumn},
      {tr("Maker"), &SConfig::GetInstance().m_showMakerColumn},
      {tr("Size"), &SConfig::GetInstance().m_showSizeColumn},
      {tr("Country"), &SConfig::GetInstance().m_showRegionColumn},
      {tr("State"), &SConfig::GetInstance().m_showStateColumn}};

  QActionGroup* column_group = new QActionGroup(this);
  QMenu* cols_menu = view_menu->addMenu(tr("List Columns"));
  column_group->setExclusive(false);

  for (const auto& key : columns.keys())
  {
    bool* config = columns[key];
    QAction* action = column_group->addAction(cols_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit ColumnVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowPlatformsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> platform_map{
      {tr("Show Wii"), &SConfig::GetInstance().m_ListWii},
      {tr("Show GameCube"), &SConfig::GetInstance().m_ListGC},
      {tr("Show WAD"), &SConfig::GetInstance().m_ListWad},
      {tr("Show ELF/DOL"), &SConfig::GetInstance().m_ListElfDol}};

  QActionGroup* platform_group = new QActionGroup(this);
  QMenu* plat_menu = view_menu->addMenu(tr("Show Platforms"));
  platform_group->setExclusive(false);

  for (const auto& key : platform_map.keys())
  {
    bool* config = platform_map[key];
    QAction* action = platform_group->addAction(plat_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit GameListPlatformVisibilityToggled(key, value);
    });
  }
}

void MenuBar::AddShowRegionsMenu(QMenu* view_menu)
{
  static const QMap<QString, bool*> region_map{
      {tr("Show JAP"), &SConfig::GetInstance().m_ListJap},
      {tr("Show PAL"), &SConfig::GetInstance().m_ListPal},
      {tr("Show USA"), &SConfig::GetInstance().m_ListUsa},
      {tr("Show Australia"), &SConfig::GetInstance().m_ListAustralia},
      {tr("Show France"), &SConfig::GetInstance().m_ListFrance},
      {tr("Show Germany"), &SConfig::GetInstance().m_ListGermany},
      {tr("Show Italy"), &SConfig::GetInstance().m_ListItaly},
      {tr("Show Korea"), &SConfig::GetInstance().m_ListKorea},
      {tr("Show Netherlands"), &SConfig::GetInstance().m_ListNetherlands},
      {tr("Show Russia"), &SConfig::GetInstance().m_ListRussia},
      {tr("Show Spain"), &SConfig::GetInstance().m_ListSpain},
      {tr("Show Taiwan"), &SConfig::GetInstance().m_ListTaiwan},
      {tr("Show World"), &SConfig::GetInstance().m_ListWorld},
      {tr("Show Unknown"), &SConfig::GetInstance().m_ListUnknown}};

  QActionGroup* region_group = new QActionGroup(this);
  QMenu* region_menu = view_menu->addMenu(tr("Show Regions"));
  region_group->setExclusive(false);

  for (const auto& key : region_map.keys())
  {
    bool* config = region_map[key];
    QAction* action = region_group->addAction(region_menu->addAction(key));
    action->setCheckable(true);
    action->setChecked(*config);
    connect(action, &QAction::toggled, [this, config, key](bool value) {
      *config = value;
      emit GameListRegionVisibilityToggled(key, value);
    });
  }
}

void MenuBar::UpdateToolsMenu(bool emulation_started)
{
  m_boot_sysmenu->setEnabled(!emulation_started);
  m_perform_online_update_menu->setEnabled(!emulation_started);
  m_ntscj_ipl->setEnabled(!emulation_started &&
                          File::Exists(SConfig::GetInstance().GetBootROMPath(JAP_DIR)));
  m_ntscu_ipl->setEnabled(!emulation_started &&
                          File::Exists(SConfig::GetInstance().GetBootROMPath(USA_DIR)));
  m_pal_ipl->setEnabled(!emulation_started &&
                        File::Exists(SConfig::GetInstance().GetBootROMPath(EUR_DIR)));
  m_import_backup->setEnabled(!emulation_started);

  if (!emulation_started)
  {
    IOS::HLE::Kernel ios;
    const auto tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);

    const QString sysmenu_version =
        tmd.IsValid() ?
            QString::fromStdString(DiscIO::GetSysMenuVersionString(tmd.GetTitleVersion())) :
            QStringLiteral("");
    m_boot_sysmenu->setText(tr("Load Wii System Menu %1").arg(sysmenu_version));

    m_boot_sysmenu->setEnabled(tmd.IsValid());

    for (QAction* action : m_perform_online_update_menu->actions())
      action->setEnabled(!tmd.IsValid());
    m_perform_online_update_for_current_region->setEnabled(tmd.IsValid());
  }
}

void MenuBar::InstallWAD()
{
  QString wad_file = QFileDialog::getOpenFileName(this, tr("Select a title to install to NAND"),
                                                  QString(), tr("WAD files (*.wad)"));

  if (wad_file.isEmpty())
    return;

  QMessageBox result_dialog(this);

  if (GameFile(wad_file).Install())
  {
    result_dialog.setIcon(QMessageBox::Information);
    result_dialog.setText(tr("Successfully installed this title to the NAND."));
  }
  else
  {
    result_dialog.setIcon(QMessageBox::Critical);
    result_dialog.setText(tr("Failed to install this title to the NAND."));
  }

  result_dialog.exec();
}

void MenuBar::ImportWiiSave()
{
  QString file = QFileDialog::getOpenFileName(this, tr("Select the save file"), QDir::currentPath(),
                                              tr("Wii save files (*.bin);;"
                                                 "All Files (*)"));

  if (!file.isEmpty())
    CWiiSaveCrypted::ImportWiiSave(file.toStdString());
}

void MenuBar::ExportWiiSaves()
{
  CWiiSaveCrypted::ExportAllSaves();
}

void MenuBar::NANDExtractCertificates()
{
  if (DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX)))
  {
    QMessageBox::information(this, tr("Success"),
                             tr("Successfully extracted certificates from NAND"));
  }
  else
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to extract certificates from NAND"));
  }
}
