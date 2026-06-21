// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/GBATASInputWindow.h"

#include <string>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/HW/GBAPad.h"
#include "Core/HW/GBAPadEmu.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DolphinQt/Scripting/ScriptFavoritesWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/InputConfig.h"

GBATASInputWindow::GBATASInputWindow(QWidget* parent, int controller_id)
    : TASInputWindow(parent), m_controller_id(controller_id)
{
  setWindowTitle(tr("GBA TAS Input %1").arg(controller_id + 1));
  SetAlwaysOnTopConfigKey("GBA.AlwaysOnTop." + std::to_string(controller_id));
  m_turbo_press_frames->setValue(1);
  m_turbo_release_frames->setValue(1);
  m_toggle_lines->hide();

  m_b_button =
      CreateButton(QStringLiteral("&B"), GBAPad::BUTTONS_GROUP, GBAPad::B_BUTTON, &m_overrider);
  m_a_button =
      CreateButton(QStringLiteral("&A"), GBAPad::BUTTONS_GROUP, GBAPad::A_BUTTON, &m_overrider);
  m_l_button =
      CreateButton(QStringLiteral("&L"), GBAPad::BUTTONS_GROUP, GBAPad::L_BUTTON, &m_overrider);
  m_r_button =
      CreateButton(QStringLiteral("&R"), GBAPad::BUTTONS_GROUP, GBAPad::R_BUTTON, &m_overrider);
  m_select_button = CreateButton(QStringLiteral("SELE&CT"), GBAPad::BUTTONS_GROUP,
                                 GBAPad::SELECT_BUTTON, &m_overrider);
  m_start_button = CreateButton(QStringLiteral("&START"), GBAPad::BUTTONS_GROUP,
                                GBAPad::START_BUTTON, &m_overrider);

  m_left_button =
      CreateButton(QStringLiteral("L&eft"), GBAPad::DPAD_GROUP, DIRECTION_LEFT, &m_overrider);
  m_up_button = CreateButton(QStringLiteral("&Up"), GBAPad::DPAD_GROUP, DIRECTION_UP, &m_overrider);
  m_down_button =
      CreateButton(QStringLiteral("&Down"), GBAPad::DPAD_GROUP, DIRECTION_DOWN, &m_overrider);
  m_right_button =
      CreateButton(QStringLiteral("R&ight"), GBAPad::DPAD_GROUP, DIRECTION_RIGHT, &m_overrider);

  auto* buttons_layout = new QGridLayout;

  buttons_layout->addWidget(m_left_button, 0, 0);
  buttons_layout->addWidget(m_up_button, 0, 1);
  buttons_layout->addWidget(m_down_button, 0, 2);
  buttons_layout->addWidget(m_right_button, 0, 3);

  buttons_layout->addWidget(m_l_button, 1, 0);
  buttons_layout->addWidget(m_r_button, 1, 1);
  buttons_layout->addWidget(m_b_button, 1, 2);
  buttons_layout->addWidget(m_a_button, 1, 3);

  buttons_layout->addWidget(m_select_button, 2, 0, 1, 2);
  buttons_layout->addWidget(m_start_button, 2, 2, 1, 2);

  buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 4);

  QGroupBox* buttons_box = new QGroupBox(tr("Buttons"));
  buttons_box->setLayout(buttons_layout);

  auto* favorites_widget = new ScriptFavoritesWidget(this);
  favorites_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  favorites_widget->setFixedHeight(buttons_box->sizeHint().height());

  m_disconnect_checkbox = new QCheckBox(tr("Disconnect"));
  m_disconnect_checkbox->setToolTip(
      tr("While checked, the GBA link cable is force-disconnected for this port during "
         "recording/playback."));
  connect(m_disconnect_checkbox, &QCheckBox::toggled, this,
          [this](bool checked) { Pad::SetGBAForceDisconnect(m_controller_id, checked); });
  buttons_layout->addWidget(m_disconnect_checkbox, 3, 0, 1, 4);
  favorites_widget->setFixedHeight(buttons_box->sizeHint().height());

  auto* buttons_and_favorites = new QHBoxLayout;
  buttons_and_favorites->setAlignment(Qt::AlignTop);
  buttons_and_favorites->addWidget(buttons_box, 1, Qt::AlignTop);
  buttons_and_favorites->addWidget(favorites_widget, 0, Qt::AlignTop);

  auto* layout = new QVBoxLayout;
  layout->addLayout(buttons_and_favorites);
  layout->addWidget(m_settings_box);

  SetResizableContentLayout(layout);

  RegisterVisibilitySection(tr("Buttons"), "GBA.Buttons", buttons_box);
  RegisterVisibilitySection(tr("Settings"), "GBA.Settings", m_settings_box);
  RegisterVisibilitySection(tr("Favorite Scripts"), "GBA.FavoriteScripts", favorites_widget);
  FinalizeVisibilitySections();

  MakeSectionResizable("GBA.Buttons", buttons_box);
  MakeSectionResizable("GBA.FavoriteScripts", favorites_widget);
  MakeSectionResizable("GBA.Settings", m_settings_box);
}

void GBATASInputWindow::hideEvent(QHideEvent* event)
{
  Pad::GetGBAConfig()->GetController(m_controller_id)->ClearInputOverrideFunction();
  TASInputWindow::hideEvent(event);
}

void GBATASInputWindow::showEvent(QShowEvent* event)
{
  Pad::GetGBAConfig()
      ->GetController(m_controller_id)
      ->SetInputOverrideFunction(m_overrider.GetInputOverrideFunction());
  if (m_disconnect_checkbox)
    m_disconnect_checkbox->setChecked(Pad::GetGBAForceDisconnect(m_controller_id));
  TASInputWindow::showEvent(event);
}

void GBATASInputWindow::UpdateLiveInputDisplay()
{
  const auto status = Core::System::GetInstance().GetMovie().GetDisplayedPadStatus(m_controller_id);
  if (!status.has_value())
    return;

  const GCPadStatus& pad_status = *status;
  m_b_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_B) != 0);
  m_a_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_A) != 0);
  m_l_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_L) != 0);
  m_r_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_R) != 0);
  m_select_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_Z) != 0);
  m_start_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_START) != 0);
  m_left_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_LEFT) != 0);
  m_up_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_UP) != 0);
  m_down_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_DOWN) != 0);
  m_right_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_RIGHT) != 0);
  if (m_disconnect_checkbox)
    m_disconnect_checkbox->setChecked((pad_status.button & PAD_BUTTON_Y) != 0);
}
