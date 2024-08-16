// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/FreeLookWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
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

  m_freelook_background_input = new QCheckBox(tr("Background Input"));
  m_freelook_background_input->setChecked(Config::Get(Config::FREE_LOOK_BACKGROUND_INPUT));

  auto* hlayout = new QHBoxLayout();
  hlayout->addWidget(new QLabel(tr("Camera 1")));
  hlayout->addWidget(m_freelook_control_type);
  hlayout->addWidget(m_freelook_controller_configure_button);

  layout->addWidget(m_enable_freelook);
  layout->addLayout(hlayout);
  layout->addWidget(m_freelook_background_input);
  layout->addWidget(description);

  setLayout(layout);
}

void FreeLookWidget::ConnectWidgets()
{
  connect(m_freelook_controller_configure_button, &QPushButton::clicked, this,
          &FreeLookWidget::OnFreeLookControllerConfigured);
  connect(m_enable_freelook, &QCheckBox::clicked, this, &FreeLookWidget::SaveSettings);
  connect(m_freelook_background_input, &QCheckBox::clicked, this, &FreeLookWidget::SaveSettings);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    const QSignalBlocker blocker(this);
    LoadSettings();
  });
}

void FreeLookWidget::OnFreeLookControllerConfigured()
{
  if (m_freelook_controller_configure_button != QObject::sender())
    return;
  constexpr int index = 0;
  MappingWindow* window = new MappingWindow(this, MappingWindow::Type::MAPPING_FREELOOK, index);
  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->setWindowModality(Qt::WindowModality::WindowModal);
  SetQWidgetWindowDecorations(window);
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
  m_freelook_background_input->setEnabled(checked);
}

void FreeLookWidget::SaveSettings()
{
  const bool checked = m_enable_freelook->isChecked();
  Config::SetBaseOrCurrent(Config::FREE_LOOK_ENABLED, checked);
  Config::SetBaseOrCurrent(Config::FREE_LOOK_BACKGROUND_INPUT,
                           m_freelook_background_input->isChecked());
  m_freelook_control_type->setEnabled(checked);
  m_freelook_controller_configure_button->setEnabled(checked);
  m_freelook_background_input->setEnabled(checked);
}
