// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/SettingsWindow.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QVBoxLayout>

#include "Common/EnumUtils.h"

#include "DolphinQt/Config/ControllersPane.h"
#include "DolphinQt/Config/Graphics/GraphicsPane.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/QtUtils/QtUtils.h"
#include "DolphinQt/QtUtils/VerticalTabsTabWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"
#include "DolphinQt/Settings/AdvancedPane.h"
#include "DolphinQt/Settings/AudioPane.h"
#include "DolphinQt/Settings/GameCubePane.h"
#include "DolphinQt/Settings/GeneralPane.h"
#include "DolphinQt/Settings/InterfacePane.h"
#include "DolphinQt/Settings/PathPane.h"
#include "DolphinQt/Settings/WiiPane.h"

TabbedSettingsWindow::TabbedSettingsWindow(QWidget* const parent) : QDialog{parent}
{
  auto* const layout = new QVBoxLayout{this};

  m_tabbed_panes = new QtUtils::VerticalTabsTabWidget();

  layout->addWidget(m_tabbed_panes);

  // The QFrame gives us some padding around the button.
  auto* const button_frame = new QFrame;
  auto* const button_layout = new QGridLayout{button_frame};
  auto* const button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  layout->addWidget(button_frame);
  button_layout->addWidget(button_box);
  button_layout->setContentsMargins(QMargins{});

  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TabbedSettingsWindow::OnDoneCreatingPanes()
{
  // Make sure the first item is actually selected by default.
  ActivatePane(0);
  // Take on the preferred size.
  QtUtils::AdjustSizeWithinScreen(this);
}

void TabbedSettingsWindow::AddPane(QWidget* const widget, const QString& name)
{
  m_tabbed_panes->addTab(widget, name);
}

void TabbedSettingsWindow::AddWrappedPane(QWidget* const widget, const QString& name)
{
  AddPane(GetWrappedWidget(widget), name);
}

void TabbedSettingsWindow::ActivatePane(const int index)
{
  m_tabbed_panes->setCurrentIndex(index);
}

SettingsWindow::SettingsWindow(MainWindow* const parent) : TabbedSettingsWindow{parent}
{
  setWindowTitle(tr("Settings"));

  // If you change the order, don't forget to update the SettingsWindowPaneIndex enum.
  AddWrappedPane(new GeneralPane, tr("General"));
  AddPane(new GraphicsPane{parent, nullptr}, tr("Graphics"));
  AddWrappedPane(new ControllersPane, tr("Controllers"));
  AddWrappedPane(new InterfacePane, tr("Interface"));
  AddWrappedPane(new AudioPane, tr("Audio"));
  AddWrappedPane(new PathPane, tr("Paths"));
  AddWrappedPane(new GameCubePane, tr("GameCube"));
  AddWrappedPane(new WiiPane, tr("Wii"));
  AddWrappedPane(new AdvancedPane, tr("Advanced"));

  OnDoneCreatingPanes();
}

void SettingsWindow::SelectPane(SettingsWindowPaneIndex index)
{
  ActivatePane(Common::ToUnderlying(index));
}
