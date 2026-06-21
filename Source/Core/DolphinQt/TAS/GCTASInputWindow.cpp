// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/GCTASInputWindow.h"

#include <algorithm>
#include <array>
#include <string>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/FlowLayout.h"
#include "DolphinQt/Scripting/ScriptFavoritesWidget.h"
#include "DolphinQt/TAS/StickWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/InputConfig.h"

namespace
{
struct MainStickPreset
{
  int x;
  int y;
};

constexpr std::array<MainStickPreset, 9> k_default_main_ess = {
    MainStickPreset{111, 145},  // Up-left
    {128, 146},                 // Up
    {145, 145},                 // Up-right
    {110, 128},                 // Left
    {128, 128},                 // Center
    {146, 128},                 // Right
    {111, 111},                 // Down-left
    {128, 110},                 // Down
    {145, 111},                 // Down-right
};

GCTASInputWindow* s_gc_tas_windows[4] = {nullptr, nullptr, nullptr, nullptr};
}  // namespace

GCTASInputWindow::GCTASInputWindow(QWidget* parent, int controller_id)
    : TASInputWindow(parent), m_controller_id(controller_id)
{
  if (m_controller_id >= 0 && m_controller_id < 4)
    s_gc_tas_windows[m_controller_id] = this;

  setWindowTitle(tr("GameCube TAS Input %1").arg(controller_id + 1));
  SetAlwaysOnTopConfigKey("GC.AlwaysOnTop." + std::to_string(controller_id));

  StickWidget* main_stick_widget = nullptr;
  StickWidget* c_stick_widget = nullptr;
  m_main_stick_box = CreateStickInputs(tr("Main Stick"), GCPad::MAIN_STICK_GROUP, &m_overrider, 1,
                                       1, 255, 255, Qt::Key_F, Qt::Key_G, &m_main_stick_x_value,
                                       &m_main_stick_y_value, &main_stick_widget);
  m_c_stick_box = CreateStickInputs(tr("C Stick"), GCPad::C_STICK_GROUP, &m_overrider, 1, 1, 255,
                                    255, Qt::Key_H, Qt::Key_J, &m_c_stick_x_value,
                                    &m_c_stick_y_value, &c_stick_widget);

  if (main_stick_widget)
    main_stick_widget->setMinimumSize(16, 16);
  if (c_stick_widget)
    c_stick_widget->setMinimumSize(16, 16);

  // The sticks own the top row and split it evenly, growing and shrinking with the window.
  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_main_stick_box);
  top_layout->addWidget(m_c_stick_box);

  m_triggers_box = new QGroupBox(tr("Triggers"));
  auto* l_trigger_layout = new QVBoxLayout;
  l_trigger_layout->addWidget(new QLabel(QStringLiteral("%1 (%2)")
                                             .arg(tr("Left"),
                                                  QKeySequence(Qt::ALT | Qt::Key_N)
                                                      .toString(QKeySequence::NativeText))));
  m_l_trigger_value = CreateSliderValuePair(GCPad::TRIGGERS_GROUP, GCPad::L_ANALOG, &m_overrider,
                                            l_trigger_layout, 0, 0, 0, 255,
                                            QKeySequence(Qt::ALT | Qt::Key_N), Qt::Vertical,
                                            m_triggers_box);

  auto* r_trigger_layout = new QVBoxLayout;
  r_trigger_layout->addWidget(new QLabel(QStringLiteral("%1 (%2)")
                                             .arg(tr("Right"),
                                                  QKeySequence(Qt::ALT | Qt::Key_M)
                                                      .toString(QKeySequence::NativeText))));
  m_r_trigger_value = CreateSliderValuePair(GCPad::TRIGGERS_GROUP, GCPad::R_ANALOG, &m_overrider,
                                            r_trigger_layout, 0, 0, 0, 255,
                                            QKeySequence(Qt::ALT | Qt::Key_M), Qt::Vertical,
                                            m_triggers_box);

  auto* triggers_layout = new QHBoxLayout;
  triggers_layout->setAlignment(Qt::AlignTop);
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  m_triggers_box->setLayout(triggers_layout);

  m_a_button =
      CreateButton(QStringLiteral("&A"), GCPad::BUTTONS_GROUP, GCPad::A_BUTTON, &m_overrider);
  m_b_button =
      CreateButton(QStringLiteral("&B"), GCPad::BUTTONS_GROUP, GCPad::B_BUTTON, &m_overrider);
  m_x_button =
      CreateButton(QStringLiteral("&X"), GCPad::BUTTONS_GROUP, GCPad::X_BUTTON, &m_overrider);
  m_y_button =
      CreateButton(QStringLiteral("&Y"), GCPad::BUTTONS_GROUP, GCPad::Y_BUTTON, &m_overrider);
  m_z_button =
      CreateButton(QStringLiteral("&Z"), GCPad::BUTTONS_GROUP, GCPad::Z_BUTTON, &m_overrider);
  m_start_button = CreateButton(QStringLiteral("&START"), GCPad::BUTTONS_GROUP, GCPad::START_BUTTON,
                                &m_overrider);

  m_l_button =
      CreateButton(QStringLiteral("&L"), GCPad::TRIGGERS_GROUP, GCPad::L_DIGITAL, &m_overrider);
  m_r_button =
      CreateButton(QStringLiteral("&R"), GCPad::TRIGGERS_GROUP, GCPad::R_DIGITAL, &m_overrider);

  m_left_button =
      CreateButton(QStringLiteral("L&eft"), GCPad::DPAD_GROUP, DIRECTION_LEFT, &m_overrider);
  m_up_button = CreateButton(QStringLiteral("&Up"), GCPad::DPAD_GROUP, DIRECTION_UP, &m_overrider);
  m_down_button =
      CreateButton(QStringLiteral("&Down"), GCPad::DPAD_GROUP, DIRECTION_DOWN, &m_overrider);
  m_right_button =
      CreateButton(QStringLiteral("R&ight"), GCPad::DPAD_GROUP, DIRECTION_RIGHT, &m_overrider);

  auto* buttons_layout = new QGridLayout;
  buttons_layout->addWidget(m_a_button, 0, 0);
  buttons_layout->addWidget(m_b_button, 0, 1);
  buttons_layout->addWidget(m_x_button, 0, 2);
  buttons_layout->addWidget(m_y_button, 0, 3);
  buttons_layout->addWidget(m_l_button, 1, 0);
  buttons_layout->addWidget(m_r_button, 1, 1);
  buttons_layout->addWidget(m_z_button, 1, 2);
  buttons_layout->addWidget(m_start_button, 1, 3);

  auto* dpad_layout = new QGridLayout;
  dpad_layout->addWidget(m_left_button, 1, 0);
  dpad_layout->addWidget(m_up_button, 0, 1);
  dpad_layout->addWidget(m_down_button, 2, 1);
  dpad_layout->addWidget(m_right_button, 1, 2);
  dpad_layout->setVerticalSpacing(0);
  dpad_layout->setHorizontalSpacing(20);

  auto* buttons_content_layout = new QVBoxLayout;
  buttons_content_layout->setAlignment(Qt::AlignTop);
  buttons_content_layout->addLayout(buttons_layout);
  buttons_content_layout->addLayout(dpad_layout);

  m_buttons_box = new QGroupBox(tr("Buttons"));
  m_buttons_box->setLayout(buttons_content_layout);

  auto* favorites_widget = new ScriptFavoritesWidget(this);
  favorites_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  favorites_widget->setFixedHeight(m_buttons_box->sizeHint().height());

  // The remaining sections flow left-to-right and wrap to the next row when the window is too
  // narrow, instead of overflowing offscreen.
  auto* bottom_layout = new FlowLayout;
  bottom_layout->addWidget(m_triggers_box);
  bottom_layout->addWidget(m_buttons_box);
  bottom_layout->addWidget(favorites_widget);
  bottom_layout->addWidget(m_settings_box);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout, 1);
  layout->addLayout(bottom_layout);
  SetResizableContentLayout(layout);

  RegisterVisibilitySection(tr("Main Stick"), "GC.MainStick", m_main_stick_box);
  RegisterVisibilitySection(tr("C Stick"), "GC.CStick", m_c_stick_box);
  RegisterVisibilitySection(tr("Triggers"), "GC.Triggers", m_triggers_box);
  RegisterVisibilitySection(tr("Buttons"), "GC.Buttons", m_buttons_box);
  RegisterVisibilitySection(tr("Settings"), "GC.Settings", m_settings_box);
  RegisterVisibilitySection(tr("Favorite Scripts"), "GC.FavoriteScripts", favorites_widget);
  FinalizeVisibilitySections();

  MakeSectionResizable("GC.Triggers", m_triggers_box);
  MakeSectionResizable("GC.Buttons", m_buttons_box);
  MakeSectionResizable("GC.FavoriteScripts", favorites_widget);
  MakeSectionResizable("GC.Settings", m_settings_box);

  if (m_toggle_lines && main_stick_widget)
    connect(m_toggle_lines, &QCheckBox::toggled, main_stick_widget, &StickWidget::SetAxisLines);
  if (m_toggle_lines && c_stick_widget)
    connect(m_toggle_lines, &QCheckBox::toggled, c_stick_widget, &StickWidget::SetAxisLines);
}

GCTASInputWindow::~GCTASInputWindow()
{
  if (m_controller_id >= 0 && m_controller_id < 4 && s_gc_tas_windows[m_controller_id] == this)
    s_gc_tas_windows[m_controller_id] = nullptr;
}

GCTASInputWindow* GCTASInputWindow::GetInstanceForController(int controller_id)
{
  if (controller_id < 0 || controller_id >= 4)
    return nullptr;
  return s_gc_tas_windows[controller_id];
}

void GCTASInputWindow::ApplyEssPreset(int preset_index)
{
  if (!m_main_stick_x_value || !m_main_stick_y_value)
    return;

  if (preset_index < 0 || preset_index >= static_cast<int>(k_default_main_ess.size()))
    return;

  int x = k_default_main_ess[preset_index].x;
  int y = k_default_main_ess[preset_index].y;

  Common::IniFile ini;
  const std::string ini_path = File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini";
  ini.Load(ini_path);
  ini.GetIfExists("TAS", "MainStickEss" + std::to_string(preset_index) + "X", &x);
  ini.GetIfExists("TAS", "MainStickEss" + std::to_string(preset_index) + "Y", &y);

  x = std::clamp(x, 0, 255);
  y = std::clamp(y, 0, 255);

  m_main_stick_x_value->setValue(x);
  m_main_stick_y_value->setValue(y);
}

void GCTASInputWindow::hideEvent(QHideEvent* event)
{
  Pad::GetConfig()->GetController(m_controller_id)->ClearInputOverrideFunction();
}

void GCTASInputWindow::showEvent(QShowEvent* event)
{
  Pad::GetConfig()
      ->GetController(m_controller_id)
      ->SetInputOverrideFunction(m_overrider.GetInputOverrideFunction());
}

void GCTASInputWindow::UpdateLiveInputDisplay()
{
  const auto status = Core::System::GetInstance().GetMovie().GetDisplayedPadStatus(m_controller_id);
  if (!status.has_value())
    return;

  const GCPadStatus& pad_status = *status;
  m_a_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_A) != 0);
  m_b_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_B) != 0);
  m_x_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_X) != 0);
  m_y_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_Y) != 0);
  m_z_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_Z) != 0);
  m_l_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_L) != 0);
  m_r_button->OnControllerValueChanged((pad_status.button & PAD_TRIGGER_R) != 0);
  m_start_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_START) != 0);
  m_left_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_LEFT) != 0);
  m_up_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_UP) != 0);
  m_down_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_DOWN) != 0);
  m_right_button->OnControllerValueChanged((pad_status.button & PAD_BUTTON_RIGHT) != 0);

  if (m_main_stick_x_value)
    m_main_stick_x_value->OnControllerValueChanged(pad_status.stickX);
  if (m_main_stick_y_value)
    m_main_stick_y_value->OnControllerValueChanged(pad_status.stickY);
  if (m_c_stick_x_value)
    m_c_stick_x_value->OnControllerValueChanged(pad_status.substickX);
  if (m_c_stick_y_value)
    m_c_stick_y_value->OnControllerValueChanged(pad_status.substickY);
  if (m_l_trigger_value)
    m_l_trigger_value->OnControllerValueChanged(pad_status.triggerLeft);
  if (m_r_trigger_value)
    m_r_trigger_value->OnControllerValueChanged(pad_status.triggerRight);
}
