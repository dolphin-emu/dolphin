// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CommonControllersWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/Config/SDLHints/SDLHintsWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

CommonControllersWidget::CommonControllersWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();

  connect(&Settings::Instance(), &Settings::ConfigChanged, this,
          &CommonControllersWidget::LoadSettings);
}

void CommonControllersWidget::CreateLayout()
{
  // i18n: This is "common" as in "shared", not the opposite of "uncommon"
  m_common_box = new QGroupBox(tr("Common"));
  m_common_layout = new QVBoxLayout();
  m_common_bg_input = new QCheckBox(tr("Background Input"));
  m_common_auto_controller_assignment = new QCheckBox(tr("Auto Controller Assignment"));
  m_common_auto_controller_assignment->setToolTip(
      tr("Automatically assign connected SDL controllers to player slots in connection order.\n"
         "When a controller is disconnected, its slot becomes inactive.\n"
         "When a controller is connected, it will be assigned to the next available slot."));
  m_common_configure_controller_interface =
      new NonDefaultQPushButton(tr("Alternate Input Sources"));
  m_common_configure_sdl_hints = new NonDefaultQPushButton(tr("SDL Controller Settings"));

  m_common_layout->addWidget(m_common_bg_input);
  m_common_layout->addWidget(m_common_auto_controller_assignment);
  m_common_layout->addWidget(m_common_configure_controller_interface);
  m_common_layout->addWidget(m_common_configure_sdl_hints);

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

void CommonControllersWidget::ConnectWidgets()
{
  connect(m_common_bg_input, &QCheckBox::toggled, this, &CommonControllersWidget::SaveSettings);
  connect(m_common_auto_controller_assignment, &QCheckBox::toggled, this,
          &CommonControllersWidget::SaveSettings);
  connect(m_common_configure_controller_interface, &QPushButton::clicked, this,
          &CommonControllersWidget::OnControllerInterfaceConfigure);
  connect(m_common_configure_sdl_hints, &QPushButton::clicked, this,
          &CommonControllersWidget::OnSDLHintConfigure);
}

void CommonControllersWidget::OnControllerInterfaceConfigure()
{
  ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void CommonControllersWidget::OnSDLHintConfigure()
{
  SDLHintsWindow* window = new SDLHintsWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void CommonControllersWidget::LoadSettings()
{
  SignalBlocking(m_common_bg_input)->setChecked(Config::Get(Config::MAIN_INPUT_BACKGROUND_INPUT));
  SignalBlocking(m_common_auto_controller_assignment)
      ->setChecked(Config::Get(Config::MAIN_AUTO_CONTROLLER_ASSIGNMENT));
}

void CommonControllersWidget::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::MAIN_INPUT_BACKGROUND_INPUT, m_common_bg_input->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_AUTO_CONTROLLER_ASSIGNMENT,
                           m_common_auto_controller_assignment->isChecked());
  Config::Save();
}
