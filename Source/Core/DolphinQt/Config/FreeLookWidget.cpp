// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/FreeLookWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/Config/InputFocus.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/Settings.h"

FreeLookWidget::FreeLookWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void FreeLookWidget::CreateLayout()
{
  auto* layout = new QVBoxLayout();

  m_enable_freelook = new ToolTipCheckBox(tr("Enable"));
  m_enable_freelook->setChecked(Config::Get(Config::FREE_LOOK_ENABLED));
  m_enable_freelook->SetDescription(
      tr("Allows manipulation of the in-game camera.<br><br><dolphin_emphasis>If unsure, "
         "leave this unchecked.</dolphin_emphasis>"));
#ifdef USE_RETRO_ACHIEVEMENTS
  const bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  m_enable_freelook->setEnabled(!hardcore);
#endif  // USE_RETRO_ACHIEVEMENTS
  m_freelook_controller_configure_button = new NonDefaultQPushButton(tr("Configure Controller"));

  m_freelook_control_type = new ConfigChoice({tr("Six Axis"), tr("First Person"), tr("Orbital")},
                                             Config::FL1_CONTROL_TYPE);
  m_freelook_control_type->SetTitle(tr("Free Look Control Type"));
  m_freelook_control_type->SetDescription(tr(
      "Changes the in-game camera type during Free Look.<br><br>"
      "Six Axis: Offers full camera control on all axes, akin to moving a spacecraft in zero "
      "gravity. This is the most powerful Free Look option but is the most challenging to use.<br> "
      "<br>"
      "First Person: Controls the free camera similarly to a first person video game. The camera "
      "can rotate and travel, but roll is impossible. Easy to use, but limiting.<br><br>"
      "Orbital: Rotates the free camera around the original camera. Has no lateral movement, only "
      "rotation and you may zoom up to the camera's origin point."));

  m_freelook_accept_input_from =
      new ConfigChoice({tr("Render Window or TAS Input"), tr("Any Application"), tr("Dolphin")},
                       Config::FREE_LOOK_FOCUS_POLICY);
  m_freelook_accept_input_from->SetTitle(tr("Accept Input From"));
  m_freelook_accept_input_from->SetDescription(
      tr("Requires the selected window or application to be focused for Free Look inputs to be "
         "accepted. Some Dolphin windows such as controller or hotkey mapping windows will block "
         "inputs while they are open regardless of this setting, and text fields will block inputs "
         "while they are focused."
         "<br><br><dolphin_emphasis>If unsure, select 'Render Window or TAS "
         "Input'.</dolphin_emphasis>"));

  auto* description =
      new QLabel(tr("Free Look allows for manipulation of the in-game camera. "
                    "Different camera types are available from the dropdown.<br><br>"
                    "For detailed instructions, "
                    "<a href=\"https://wiki.dolphin-emu.org/index.php?title=Free_Look\">"
                    "refer to this page</a>."));
  description->setTextFormat(Qt::RichText);
  description->setWordWrap(true);
  description->setTextInteractionFlags(Qt::TextBrowserInteraction);
  description->setOpenExternalLinks(true);

  auto* const form_layout = new QFormLayout();
  auto* const hlayout = new QHBoxLayout();
  hlayout->addWidget(m_freelook_control_type);
  hlayout->addWidget(m_freelook_controller_configure_button);
  form_layout->addRow(new QLabel(tr("Camera 1")), hlayout);

  form_layout->addRow(new QLabel(tr("Accept Input From:")), m_freelook_accept_input_from);

  layout->addWidget(m_enable_freelook);
  layout->addLayout(hlayout);
  layout->addLayout(form_layout);
  layout->addWidget(description);

  setLayout(layout);
}

void FreeLookWidget::ConnectWidgets()
{
  connect(m_freelook_controller_configure_button, &QPushButton::clicked, this,
          &FreeLookWidget::OnFreeLookControllerConfigured);
  connect(m_enable_freelook, &QCheckBox::clicked, this, &FreeLookWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    const QSignalBlocker blocker(this);
    LoadSettings();
  });
}

void FreeLookWidget::OnFreeLookControllerConfigured()
{
  if (m_freelook_controller_configure_button != QObject::sender())
    return;
  const int index = 0;
  MappingWindow* window = new MappingWindow(this, MappingWindow::Type::MAPPING_FREELOOK, index);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  window->show();
}

void FreeLookWidget::LoadSettings()
{
  const bool checked = Config::Get(Config::FREE_LOOK_ENABLED);
  m_enable_freelook->setChecked(checked);
#ifdef USE_RETRO_ACHIEVEMENTS
  const bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  m_enable_freelook->setEnabled(!hardcore);
#endif  // USE_RETRO_ACHIEVEMENTS
  m_freelook_control_type->setEnabled(checked);
  m_freelook_controller_configure_button->setEnabled(checked);
  m_freelook_accept_input_from->setEnabled(checked);
}

void FreeLookWidget::SaveSettings()
{
  const bool checked = m_enable_freelook->isChecked();
  Config::SetBaseOrCurrent(Config::FREE_LOOK_ENABLED, checked);
  m_freelook_control_type->setEnabled(checked);
  m_freelook_controller_configure_button->setEnabled(checked);
  m_freelook_accept_input_from->setEnabled(checked);
}
