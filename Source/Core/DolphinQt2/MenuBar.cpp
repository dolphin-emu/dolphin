// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMap>
#include <QMessageBox>
#include <QUrl>

#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/State.h"
#include "DolphinQt2/AboutDialog.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/MenuBar.h"
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
  QMenu* file_menu = addMenu(tr("File"));
  m_open_action = file_menu->addAction(tr("Open"), this, SIGNAL(Open()));
  m_exit_action = file_menu->addAction(tr("Exit"), this, SIGNAL(Exit()));
}

void MenuBar::AddToolsMenu()
{
  QMenu* tools_menu = addMenu(tr("Tools"));
  m_wad_install_action = tools_menu->addAction(tr("Install WAD..."), this, SLOT(InstallWAD()));

  // Label will be set by a NANDRefresh later
  m_boot_sysmenu = tools_menu->addAction(QStringLiteral(""), [this] { emit BootWiiSystemMenu(); });
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
  QMenu* emu_menu = addMenu(tr("Emulation"));
  m_play_action = emu_menu->addAction(tr("Play"), this, SIGNAL(Play()));
  m_pause_action = emu_menu->addAction(tr("Pause"), this, SIGNAL(Pause()));
  m_stop_action = emu_menu->addAction(tr("Stop"), this, SIGNAL(Stop()));
  m_reset_action = emu_menu->addAction(tr("Reset"), this, SIGNAL(Reset()));
  m_fullscreen_action = emu_menu->addAction(tr("Fullscreen"), this, SIGNAL(Fullscreen()));
  m_frame_advance_action = emu_menu->addAction(tr("Frame Advance"), this, SIGNAL(FrameAdvance()));
  m_screenshot_action = emu_menu->addAction(tr("Take Screenshot"), this, SIGNAL(Screenshot()));
  AddStateLoadMenu(emu_menu);
  AddStateSaveMenu(emu_menu);
  AddStateSlotMenu(emu_menu);
  UpdateStateSlotMenu();
}

void MenuBar::AddStateLoadMenu(QMenu* emu_menu)
{
  m_state_load_menu = emu_menu->addMenu(tr("Load State"));
  m_state_load_menu->addAction(tr("Load State from File"), this, SIGNAL(StateLoad()));
  m_state_load_menu->addAction(tr("Load State from Selected Slot"), this, SIGNAL(StateLoadSlot()));
  m_state_load_slots_menu = m_state_load_menu->addMenu(tr("Load State from Slot"));
  m_state_load_menu->addAction(tr("Undo Load State"), this, SIGNAL(StateLoadUndo()));

  for (int i = 1; i <= 10; i++)
  {
    QAction* action = m_state_load_slots_menu->addAction(QStringLiteral(""));

    connect(action, &QAction::triggered, this, [=]() { emit StateLoadSlotAt(i); });
  }
}

void MenuBar::AddStateSaveMenu(QMenu* emu_menu)
{
  m_state_save_menu = emu_menu->addMenu(tr("Save State"));
  m_state_save_menu->addAction(tr("Save State to File"), this, SIGNAL(StateSave()));
  m_state_save_menu->addAction(tr("Save State to Selected Slot"), this, SIGNAL(StateSaveSlot()));
  m_state_save_menu->addAction(tr("Save State to Oldest Slot"), this, SIGNAL(StateSaveOldest()));
  m_state_save_slots_menu = m_state_save_menu->addMenu(tr("Save State to Slot"));
  m_state_save_menu->addAction(tr("Undo Save State"), this, SIGNAL(StateSaveUndo()));

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
    QString action_string = tr(" Slot %1 - %2").arg(slot).arg(info);
    actions_load.at(i)->setText(tr("Load from") + action_string);
    actions_save.at(i)->setText(tr("Save to") + action_string);
    actions_slot.at(i)->setText(tr("Select") + action_string);
  }
}

void MenuBar::AddViewMenu()
{
  QMenu* view_menu = addMenu(tr("View"));
  AddGameListTypeSection(view_menu);
  view_menu->addSeparator();
  AddTableColumnsMenu(view_menu);
}

void MenuBar::AddOptionsMenu()
{
  QMenu* options_menu = addMenu(tr("Options"));
  options_menu->addAction(tr("Hotkey Settings"), this, &MenuBar::ConfigureHotkeys);
}

void MenuBar::AddHelpMenu()
{
  QMenu* help_menu = addMenu(tr("Help"));
  QAction* website = help_menu->addAction(tr("Website"));
  connect(website, &QAction::triggered, this,
          []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/"))); });
  QAction* documentation = help_menu->addAction(tr("Online Documentation"));
  connect(documentation, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://dolphin-emu.org/docs/guides")));
  });
  QAction* github = help_menu->addAction(tr("GitHub Repository"));
  connect(github, &QAction::triggered, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/dolphin-emu/dolphin")));
  });

  help_menu->addSeparator();

  help_menu->addAction(tr("About"), this, SIGNAL(ShowAboutDialog()));
}

void MenuBar::AddGameListTypeSection(QMenu* view_menu)
{
  QAction* table_view = view_menu->addAction(tr("Table"));
  table_view->setCheckable(true);

  QAction* list_view = view_menu->addAction(tr("List"));
  list_view->setCheckable(true);

  QActionGroup* list_group = new QActionGroup(this);
  list_group->addAction(table_view);
  list_group->addAction(list_view);

  bool prefer_table = Settings::Instance().GetPreferredView();
  table_view->setChecked(prefer_table);
  list_view->setChecked(!prefer_table);

  connect(table_view, &QAction::triggered, this, &MenuBar::ShowTable);
  connect(list_view, &QAction::triggered, this, &MenuBar::ShowList);
}

void MenuBar::AddTableColumnsMenu(QMenu* view_menu)
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
      {tr("Quality"), &SConfig::GetInstance().m_showStateColumn}};

  QActionGroup* column_group = new QActionGroup(this);
  QMenu* cols_menu = view_menu->addMenu(tr("Table Columns"));
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

void MenuBar::UpdateToolsMenu(bool emulation_started)
{
  m_boot_sysmenu->setEnabled(!emulation_started);
  m_perform_online_update_menu->setEnabled(!emulation_started);

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
    result_dialog.setText(tr("Successfully installed title to the NAND"));
  }
  else
  {
    result_dialog.setIcon(QMessageBox::Critical);
    result_dialog.setText(tr("Failed to install title to the NAND!"));
  }

  result_dialog.exec();
}
