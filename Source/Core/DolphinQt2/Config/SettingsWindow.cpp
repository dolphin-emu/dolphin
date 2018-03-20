// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "DolphinQt2/Config/SettingsWindow.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/QtUtils/ListTabWidget.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Settings/AdvancedPane.h"
#include "DolphinQt2/Settings/AudioPane.h"
#include "DolphinQt2/Settings/GameCubePane.h"
#include "DolphinQt2/Settings/GeneralPane.h"
#include "DolphinQt2/Settings/InterfacePane.h"
#include "DolphinQt2/Settings/PathPane.h"
#include "DolphinQt2/Settings/WiiPane.h"

#include "Core/Core.h"

static int AddTab(ListTabWidget* tab_widget, const QString& label, QWidget* widget,
                  const char* icon_name)
{
  int index = tab_widget->addTab(widget, label);
  auto set_icon = [=] { tab_widget->setTabIcon(index, Resources::GetScaledThemeIcon(icon_name)); };
  QObject::connect(&Settings::Instance(), &Settings::ThemeChanged, set_icon);
  set_icon();
  return index;
}

SettingsWindow::SettingsWindow(QWidget* parent) : QDialog(parent)
{
  // Set Window Properties
  setWindowTitle(tr("Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  // Main Layout
  QVBoxLayout* layout = new QVBoxLayout;

  // Add content to layout before dialog buttons.
  m_tabs = new ListTabWidget();
  layout->addWidget(m_tabs);

  m_general_pane_index = AddTab(m_tabs, tr("General"), new GeneralPane(), "config");
  AddTab(m_tabs, tr("Interface"), new InterfacePane(), "browse");
  m_audio_pane_index = AddTab(m_tabs, tr("Audio"), new AudioPane(), "play");
  AddTab(m_tabs, tr("GameCube"), new GameCubePane(), "gcpad");
  AddTab(m_tabs, tr("Paths"), new PathPane(), "browse");

  auto* wii_pane = new WiiPane;
  AddTab(m_tabs, tr("Wii"), wii_pane, "wiimote");

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [wii_pane](Core::State state) {
    wii_pane->OnEmulationStateChanged(state != Core::State::Uninitialized);
  });

  AddTab(m_tabs, tr("Advanced"), new AdvancedPane(), "config");

  // Dialog box buttons
  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(close_box);

  setLayout(layout);
}

void SettingsWindow::SelectAudioPane()
{
  m_tabs->setCurrentIndex(m_audio_pane_index);
}

void SettingsWindow::SelectGeneralPane()
{
  m_tabs->setCurrentIndex(m_general_pane_index);
}
