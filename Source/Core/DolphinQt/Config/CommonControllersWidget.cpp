// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CommonControllersWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
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
  m_common_configure_controller_interface =
      new NonDefaultQPushButton(tr("Alternate Input Sources"));

  m_common_layout->addWidget(m_common_bg_input);
  m_common_layout->addWidget(m_common_configure_controller_interface);

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
  connect(m_common_configure_controller_interface, &QPushButton::clicked, this,
          &CommonControllersWidget::OnControllerInterfaceConfigure);
}

void CommonControllersWidget::OnControllerInterfaceConfigure()
{
  ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  SetQWidgetWindowDecorations(window);
  window->show();
}

void CommonControllersWidget::LoadSettings()
{
  SignalBlocking(m_common_bg_input)->setChecked(Config::Get(Config::MAIN_INPUT_BACKGROUND_INPUT));
}

void CommonControllersWidget::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::MAIN_INPUT_BACKGROUND_INPUT, m_common_bg_input->isChecked());
  Config::Save();
}
