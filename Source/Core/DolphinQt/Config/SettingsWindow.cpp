// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "DolphinQt/Config/SettingsWindow.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Settings/AdvancedPane.h"
#include "DolphinQt/Settings/AudioPane.h"
#include "DolphinQt/Settings/GameCubePane.h"
#include "DolphinQt/Settings/GeneralPane.h"
#include "DolphinQt/Settings/InterfacePane.h"
#include "DolphinQt/Settings/PathPane.h"
#include "DolphinQt/Settings/WiiPane.h"

#include "Core/Core.h"

SettingsWindow::SettingsWindow(QWidget* parent) : QDialog(parent)
{
  // Set Window Properties
  setWindowTitle(tr("Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  // Main Layout
  QVBoxLayout* layout = new QVBoxLayout;

  // Add content to layout before dialog buttons.
  m_tab_widget = new QTabWidget();
  layout->addWidget(m_tab_widget);

  m_tab_widget->addTab(GetWrappedWidget(new GeneralPane, this, 125, 100), tr("General"));
  m_tab_widget->addTab(GetWrappedWidget(new InterfacePane, this, 125, 100), tr("Interface"));
  m_tab_widget->addTab(GetWrappedWidget(new AudioPane, this, 125, 100), tr("Audio"));
  m_tab_widget->addTab(GetWrappedWidget(new PathPane, this, 125, 100), tr("Paths"));
  m_tab_widget->addTab(GetWrappedWidget(new GameCubePane, this, 125, 100), tr("GameCube"));

  auto* wii_pane = new WiiPane;
  m_tab_widget->addTab(GetWrappedWidget(wii_pane, this, 125, 100), tr("Wii"));

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, [wii_pane](Core::State state) {
    wii_pane->OnEmulationStateChanged(state != Core::State::Uninitialized);
  });

  m_tab_widget->addTab(new AdvancedPane(), tr("Advanced"));

  // Dialog box buttons
  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  layout->addWidget(close_box);

  setLayout(layout);
}

void SettingsWindow::SelectAudioPane()
{
  m_tab_widget->setCurrentIndex(static_cast<int>(TabIndex::Audio));
}

void SettingsWindow::SelectGeneralPane()
{
  m_tab_widget->setCurrentIndex(static_cast<int>(TabIndex::General));
}
