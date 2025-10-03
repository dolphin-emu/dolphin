// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/CommonControllersWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/Config/InputFocus.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

CommonControllersWidget::CommonControllersWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectWidgets();
}

void CommonControllersWidget::CreateLayout()
{
  // i18n: This is "common" as in "shared", not the opposite of "uncommon"
  m_common_box = new QGroupBox(tr("Common"));
  m_common_layout = new QVBoxLayout();
  m_common_accept_input_from =
      new ConfigChoice({tr("Render Window or TAS Input"), tr("Any Application"), tr("Dolphin")},
                       Config::MAIN_CONTROLLER_FOCUS_POLICY);
  m_common_accept_input_from->SetTitle(tr("Accept Input From"));
  m_common_accept_input_from->SetDescription(
      tr("Requires the selected window or application to be focused for controller inputs to be "
         "accepted. Some Dolphin windows such as controller or hotkey mapping windows will block "
         "inputs while they are open regardless of this setting, and text fields will block inputs "
         "while they are focused."
         "<br><br><dolphin_emphasis>If unsure, select 'Render Window or TAS "
         "Input'.</dolphin_emphasis>"));
  m_common_configure_controller_interface =
      new NonDefaultQPushButton(tr("Alternate Input Sources"));

  auto* const accept_input_from_layout = new QHBoxLayout();
  auto* const accept_input_from_label = new QLabel(tr("Accept Input From:"));
  accept_input_from_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  accept_input_from_layout->addWidget(accept_input_from_label);
  accept_input_from_layout->addWidget(m_common_accept_input_from);

  m_common_layout->addLayout(accept_input_from_layout);
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
  connect(m_common_configure_controller_interface, &QPushButton::clicked, this,
          &CommonControllersWidget::OnControllerInterfaceConfigure);
  connect(m_common_accept_input_from, &QComboBox::currentIndexChanged, [](const int index) {
    Settings::Instance().ControllerFocusPolicyChanged(static_cast<Config::InputFocusPolicy>(index));
  });
}

void CommonControllersWidget::OnControllerInterfaceConfigure()
{
  ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}
