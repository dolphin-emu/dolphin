// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/WiiTASInputWindow.h"

#include "InputCommon/InputConfig.h"

WiiTASInputWindow::WiiTASInputWindow(QWidget* parent, int num) : TASInputWindow(parent), m_num(num)
{
  const QKeySequence ir_x_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_F);
  const QKeySequence ir_y_shortcut_key_sequence = QKeySequence(Qt::ALT + Qt::Key_G);

  m_ir_box = new QGroupBox(QStringLiteral("%1 (%2/%3)")
                               .arg(tr("IR"),
                                    ir_x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                                    ir_y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  auto* x_layout = new QHBoxLayout;
  m_ir_x_value = CreateSliderValuePair(x_layout, ir_max_x, ir_x_shortcut_key_sequence,
                                       Qt::Horizontal, m_ir_box, true);

  auto* y_layout = new QVBoxLayout;
  m_ir_y_value = CreateSliderValuePair(y_layout, ir_max_y, ir_y_shortcut_key_sequence, Qt::Vertical,
                                       m_ir_box, true);
  m_ir_y_value->setMaximumWidth(60);

  auto* visual = new IRWidget(this);
  connect(m_ir_x_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
          &IRWidget::SetX);
  connect(m_ir_y_value, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), visual,
          &IRWidget::SetY);
  connect(visual, &IRWidget::ChangedX, m_ir_x_value, &QSpinBox::setValue);
  connect(visual, &IRWidget::ChangedY, m_ir_y_value, &QSpinBox::setValue);

  m_ir_x_value->setValue(static_cast<int>(std::round(ir_max_x / 2.)));
  m_ir_y_value->setValue(static_cast<int>(std::round(ir_max_y / 2.)));

  auto* visual_ar = new AspectRatioWidget(visual, ir_max_x, ir_max_y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* ir_layout = new QVBoxLayout;
  ir_layout->addLayout(x_layout);
  ir_layout->addLayout(visual_layout);
  m_ir_box->setLayout(ir_layout);

  m_nunchuk_stick_box = CreateStickInputs(tr("Nunchuk Stick"), m_nunchuk_stick_x_value,
                                          m_nunchuk_stick_y_value, 255, 255, Qt::Key_X, Qt::Key_Y);

  m_classic_left_stick_box =
      CreateStickInputs(tr("Left Stick"), m_classic_left_stick_x_value,
                        m_classic_left_stick_y_value, 63, 63, Qt::Key_F, Qt::Key_G);

  m_classic_right_stick_box =
      CreateStickInputs(tr("Right Stick"), m_classic_right_stick_x_value,
                        m_classic_right_stick_y_value, 31, 31, Qt::Key_Q, Qt::Key_W);

  // Need to enforce the same minimum width because otherwise the different lengths in the labels
  // used on the QGroupBox will cause the StickWidgets to have different sizes.
  m_ir_box->setMinimumWidth(20);
  m_nunchuk_stick_box->setMinimumWidth(20);

  m_remote_orientation_box = new QGroupBox(tr("Wii Remote Orientation"));

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_ir_box);
  top_layout->addWidget(m_nunchuk_stick_box);
  top_layout->addWidget(m_classic_left_stick_box);
  top_layout->addWidget(m_classic_right_stick_box);

  auto* remote_orientation_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), m_remote_orientation_x_value, 1023, Qt::Key_Q,
                                  m_remote_orientation_box);
  auto* remote_orientation_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), m_remote_orientation_y_value, 1023, Qt::Key_W,
                                  m_remote_orientation_box);
  auto* remote_orientation_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), m_remote_orientation_z_value, 1023, Qt::Key_E,
                                  m_remote_orientation_box);

  m_remote_orientation_x_value->setValue(512);
  m_remote_orientation_y_value->setValue(512);
  m_remote_orientation_z_value->setValue(616);

  auto* remote_orientation_layout = new QVBoxLayout;
  remote_orientation_layout->addLayout(remote_orientation_x_layout);
  remote_orientation_layout->addLayout(remote_orientation_y_layout);
  remote_orientation_layout->addLayout(remote_orientation_z_layout);
  m_remote_orientation_box->setLayout(remote_orientation_layout);

  m_nunchuk_orientation_box = new QGroupBox(tr("Nunchuk Orientation"));

  auto* nunchuk_orientation_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), m_nunchuk_orientation_x_value, 1023, Qt::Key_I,
                                  m_nunchuk_orientation_box);
  auto* nunchuk_orientation_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), m_nunchuk_orientation_y_value, 1023, Qt::Key_O,
                                  m_nunchuk_orientation_box);
  auto* nunchuk_orientation_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), m_nunchuk_orientation_z_value, 1023, Qt::Key_P,
                                  m_nunchuk_orientation_box);

  m_nunchuk_orientation_x_value->setValue(512);
  m_nunchuk_orientation_y_value->setValue(512);
  m_nunchuk_orientation_z_value->setValue(512);

  auto* nunchuk_orientation_layout = new QVBoxLayout;
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_x_layout);
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_y_layout);
  nunchuk_orientation_layout->addLayout(nunchuk_orientation_z_layout);
  m_nunchuk_orientation_box->setLayout(nunchuk_orientation_layout);

  m_triggers_box = new QGroupBox(tr("Triggers"));
  auto* l_trigger_layout =
      CreateSliderValuePairLayout(tr("Left"), m_left_trigger_value, 31, Qt::Key_N, m_triggers_box);
  auto* r_trigger_layout = CreateSliderValuePairLayout(tr("Right"), m_right_trigger_value, 31,
                                                       Qt::Key_M, m_triggers_box);

  auto* triggers_layout = new QVBoxLayout;
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  m_triggers_box->setLayout(triggers_layout);

  m_a_button = new QCheckBox(QStringLiteral("&A"));
  m_b_button = new QCheckBox(QStringLiteral("&B"));
  m_1_button = new QCheckBox(QStringLiteral("&1"));
  m_2_button = new QCheckBox(QStringLiteral("&2"));
  m_plus_button = new QCheckBox(QStringLiteral("&+"));
  m_minus_button = new QCheckBox(QStringLiteral("&-"));
  m_home_button = new QCheckBox(QStringLiteral("&HOME"));
  m_left_button = new QCheckBox(QStringLiteral("&Left"));
  m_up_button = new QCheckBox(QStringLiteral("&Up"));
  m_down_button = new QCheckBox(QStringLiteral("&Down"));
  m_right_button = new QCheckBox(QStringLiteral("&Right"));
  m_c_button = new QCheckBox(QStringLiteral("&C"));
  m_z_button = new QCheckBox(QStringLiteral("&Z"));

  auto* buttons_layout1 = new QHBoxLayout;
  buttons_layout1->addWidget(m_a_button);
  buttons_layout1->addWidget(m_b_button);
  buttons_layout1->addWidget(m_1_button);
  buttons_layout1->addWidget(m_2_button);
  buttons_layout1->addWidget(m_plus_button);
  buttons_layout1->addWidget(m_minus_button);

  auto* buttons_layout2 = new QHBoxLayout;
  buttons_layout2->addWidget(m_home_button);
  buttons_layout2->addWidget(m_left_button);
  buttons_layout2->addWidget(m_up_button);
  buttons_layout2->addWidget(m_down_button);
  buttons_layout2->addWidget(m_right_button);

  auto* remote_buttons_layout = new QVBoxLayout;
  remote_buttons_layout->setSizeConstraint(QLayout::SetFixedSize);
  remote_buttons_layout->addLayout(buttons_layout1);
  remote_buttons_layout->addLayout(buttons_layout2);

  m_remote_buttons_box = new QGroupBox(tr("Wii Remote Buttons"));
  m_remote_buttons_box->setLayout(remote_buttons_layout);

  auto* nunchuk_buttons_layout = new QHBoxLayout;
  nunchuk_buttons_layout->addWidget(m_c_button);
  nunchuk_buttons_layout->addWidget(m_z_button);

  m_nunchuk_buttons_box = new QGroupBox(tr("Nunchuk Buttons"));
  m_nunchuk_buttons_box->setLayout(nunchuk_buttons_layout);

  m_classic_a_button = new QCheckBox(QStringLiteral("&A"));
  m_classic_b_button = new QCheckBox(QStringLiteral("&B"));
  m_classic_x_button = new QCheckBox(QStringLiteral("&X"));
  m_classic_y_button = new QCheckBox(QStringLiteral("&Y"));
  m_classic_l_button = new QCheckBox(QStringLiteral("&L"));
  m_classic_r_button = new QCheckBox(QStringLiteral("&R"));
  m_classic_zl_button = new QCheckBox(QStringLiteral("&ZL"));
  m_classic_zr_button = new QCheckBox(QStringLiteral("ZR"));
  m_classic_plus_button = new QCheckBox(QStringLiteral("&+"));
  m_classic_minus_button = new QCheckBox(QStringLiteral("&-"));
  m_classic_home_button = new QCheckBox(QStringLiteral("&HOME"));
  m_classic_left_button = new QCheckBox(QStringLiteral("L&eft"));
  m_classic_up_button = new QCheckBox(QStringLiteral("&Up"));
  m_classic_down_button = new QCheckBox(QStringLiteral("&Down"));
  m_classic_right_button = new QCheckBox(QStringLiteral("R&ight"));

  auto* classic_buttons_layout1 = new QHBoxLayout;
  classic_buttons_layout1->addWidget(m_classic_a_button);
  classic_buttons_layout1->addWidget(m_classic_b_button);
  classic_buttons_layout1->addWidget(m_classic_x_button);
  classic_buttons_layout1->addWidget(m_classic_y_button);
  classic_buttons_layout1->addWidget(m_classic_l_button);
  classic_buttons_layout1->addWidget(m_classic_r_button);
  classic_buttons_layout1->addWidget(m_classic_zl_button);
  classic_buttons_layout1->addWidget(m_classic_zr_button);

  auto* classic_buttons_layout2 = new QHBoxLayout;
  classic_buttons_layout2->addWidget(m_classic_plus_button);
  classic_buttons_layout2->addWidget(m_classic_minus_button);
  classic_buttons_layout2->addWidget(m_classic_home_button);
  classic_buttons_layout2->addWidget(m_classic_left_button);
  classic_buttons_layout2->addWidget(m_classic_up_button);
  classic_buttons_layout2->addWidget(m_classic_down_button);
  classic_buttons_layout2->addWidget(m_classic_right_button);

  auto* classic_buttons_layout = new QVBoxLayout;
  classic_buttons_layout->setSizeConstraint(QLayout::SetFixedSize);
  classic_buttons_layout->addLayout(classic_buttons_layout1);
  classic_buttons_layout->addLayout(classic_buttons_layout2);

  m_classic_buttons_box = new QGroupBox(tr("Classic Buttons"));
  m_classic_buttons_box->setLayout(classic_buttons_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(m_remote_orientation_box);
  layout->addWidget(m_nunchuk_orientation_box);
  layout->addWidget(m_triggers_box);
  layout->addWidget(m_remote_buttons_box);
  layout->addWidget(m_nunchuk_buttons_box);
  layout->addWidget(m_classic_buttons_box);
  layout->addWidget(m_use_controller);
  layout->setAlignment(m_nunchuk_buttons_box, Qt::AlignLeft);

  setLayout(layout);

  u8 ext = 0;
  if (Core::IsRunning())
  {
    ext = static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(num))
              ->CurrentExtension();
  }
  else
  {
    IniFile ini;
    ini.Load(File::GetUserPath(D_CONFIG_IDX) + "WiimoteNew.ini");
    std::string extension;
    ini.GetIfExists("Wiimote" + std::to_string(num + 1), "Extension", &extension);

    if (extension == "Nunchuk")
      ext = 1;
    if (extension == "Classic")
      ext = 2;
  }
  UpdateExt(ext);
}

void WiiTASInputWindow::UpdateExt(u8 ext)
{
  if (ext == 1)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote + Nunchuk").arg(m_num + 1));
    m_ir_box->show();
    m_nunchuk_stick_box->show();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    m_remote_orientation_box->show();
    m_nunchuk_orientation_box->show();
    m_triggers_box->hide();
    m_nunchuk_buttons_box->show();
    m_remote_buttons_box->show();
    m_classic_buttons_box->hide();
  }
  else if (ext == 2)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Classic Controller").arg(m_num + 1));
    m_ir_box->hide();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->show();
    m_classic_left_stick_box->show();
    m_remote_orientation_box->hide();
    m_nunchuk_orientation_box->hide();
    m_triggers_box->show();
    m_remote_buttons_box->hide();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->show();
  }
  else
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote").arg(m_num + 1));
    m_ir_box->show();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    m_remote_orientation_box->show();
    m_nunchuk_orientation_box->hide();
    m_triggers_box->hide();
    m_remote_buttons_box->show();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->hide();
  }
}

void WiiTASInputWindow::GetValues(u8* report_data, WiimoteEmu::ReportFeatures rptf, int ext,
                                  wiimote_key key)
{
  if (!isVisible())
    return;

  UpdateExt(ext);

  u8* const buttons_data = rptf.core ? (report_data + rptf.core) : nullptr;
  u8* const accel_data = rptf.accel ? (report_data + rptf.accel) : nullptr;
  u8* const ir_data = rptf.ir ? (report_data + rptf.ir) : nullptr;
  u8* const ext_data = rptf.ext ? (report_data + rptf.ext) : nullptr;

  if (m_remote_buttons_box->isVisible() && buttons_data)
  {
    u16& buttons = (reinterpret_cast<wm_buttons*>(buttons_data))->hex;
    GetButton<u16>(m_a_button, buttons, WiimoteEmu::Wiimote::BUTTON_A);
    GetButton<u16>(m_b_button, buttons, WiimoteEmu::Wiimote::BUTTON_B);
    GetButton<u16>(m_1_button, buttons, WiimoteEmu::Wiimote::BUTTON_ONE);
    GetButton<u16>(m_2_button, buttons, WiimoteEmu::Wiimote::BUTTON_TWO);
    GetButton<u16>(m_plus_button, buttons, WiimoteEmu::Wiimote::BUTTON_PLUS);
    GetButton<u16>(m_minus_button, buttons, WiimoteEmu::Wiimote::BUTTON_MINUS);
    GetButton<u16>(m_home_button, buttons, WiimoteEmu::Wiimote::BUTTON_HOME);
    GetButton<u16>(m_left_button, buttons, WiimoteEmu::Wiimote::PAD_LEFT);
    GetButton<u16>(m_up_button, buttons, WiimoteEmu::Wiimote::PAD_UP);
    GetButton<u16>(m_down_button, buttons, WiimoteEmu::Wiimote::PAD_DOWN);
    GetButton<u16>(m_right_button, buttons, WiimoteEmu::Wiimote::PAD_RIGHT);
  }

  if (m_remote_orientation_box->isVisible() && accel_data && buttons_data)
  {
    wm_accel& accel = *reinterpret_cast<wm_accel*>(accel_data);
    wm_buttons& buttons = *reinterpret_cast<wm_buttons*>(buttons_data);

    u16 accel_x = (accel.x << 2) & (buttons.acc_x_lsb & 0b11);
    u16 accel_y = (accel.y << 2) & ((buttons.acc_y_lsb & 0b1) << 1);
    u16 accel_z = (accel.z << 2) & ((buttons.acc_z_lsb & 0b1) << 1);

    GetSpinBoxU16(m_remote_orientation_x_value, accel_x);
    GetSpinBoxU16(m_remote_orientation_y_value, accel_y);
    GetSpinBoxU16(m_remote_orientation_z_value, accel_z);

    accel.x = accel_x >> 2;
    accel.y = accel_y >> 2;
    accel.z = accel_z >> 2;

    buttons.acc_x_lsb = accel_x & 0b11;
    buttons.acc_y_lsb = (accel_y >> 1) & 0b1;
    buttons.acc_z_lsb = (accel_z >> 1) & 0b1;
  }

  if (m_ir_box->isVisible() && ir_data && !m_use_controller->isChecked())
  {
    u16 y = m_ir_y_value->value();
    std::array<u16, 4> x;
    x[0] = m_ir_x_value->value();
    x[1] = x[0] + 100;
    x[2] = x[0] - 10;
    x[3] = x[1] + 10;

    u8 mode;
    // Mode 5 not supported in core anyway.
    if (rptf.ext)
      mode = (rptf.ext - rptf.ir) == 10 ? 1 : 3;
    else
      mode = (rptf.size - rptf.ir) == 10 ? 1 : 3;

    if (mode == 1)
    {
      memset(ir_data, 0xFF, sizeof(wm_ir_basic) * 2);
      wm_ir_basic* const ir_basic = reinterpret_cast<wm_ir_basic*>(ir_data);
      for (int i = 0; i < 2; ++i)
      {
        if (x[i * 2] < 1024 && y < 768)
        {
          ir_basic[i].x1 = static_cast<u8>(x[i * 2]);
          ir_basic[i].x1hi = x[i * 2] >> 8;

          ir_basic[i].y1 = static_cast<u8>(y);
          ir_basic[i].y1hi = y >> 8;
        }
        if (x[i * 2 + 1] < 1024 && y < 768)
        {
          ir_basic[i].x2 = static_cast<u8>(x[i * 2 + 1]);
          ir_basic[i].x2hi = x[i * 2 + 1] >> 8;

          ir_basic[i].y2 = static_cast<u8>(y);
          ir_basic[i].y2hi = y >> 8;
        }
      }
    }
    else
    {
      // TODO: this code doesnt work, resulting in no IR TAS inputs in e.g. wii sports menu when no
      // remote extension is used
      memset(ir_data, 0xFF, sizeof(wm_ir_extended) * 4);
      wm_ir_extended* const ir_extended = reinterpret_cast<wm_ir_extended*>(ir_data);
      for (size_t i = 0; i < x.size(); ++i)
      {
        if (x[i] < 1024 && y < 768)
        {
          ir_extended[i].x = static_cast<u8>(x[i]);
          ir_extended[i].xhi = x[i] >> 8;

          ir_extended[i].y = static_cast<u8>(y);
          ir_extended[i].yhi = y >> 8;

          ir_extended[i].size = 10;
        }
      }
    }
  }

  if (ext_data && m_nunchuk_stick_box->isVisible())
  {
    wm_nc& nunchuk = *reinterpret_cast<wm_nc*>(ext_data);

    GetSpinBoxU8(m_nunchuk_stick_x_value, nunchuk.jx);
    GetSpinBoxU8(m_nunchuk_stick_y_value, nunchuk.jy);

    u16 accel_x = nunchuk.ax << 2 & (nunchuk.bt.acc_x_lsb & 0b11);
    u16 accel_y = nunchuk.ay << 2 & (nunchuk.bt.acc_y_lsb & 0b11);
    u16 accel_z = nunchuk.az << 2 & (nunchuk.bt.acc_z_lsb & 0b11);

    GetSpinBoxU16(m_nunchuk_orientation_x_value, accel_x);
    GetSpinBoxU16(m_nunchuk_orientation_y_value, accel_y);
    GetSpinBoxU16(m_nunchuk_orientation_z_value, accel_z);

    nunchuk.ax = accel_x >> 2;
    nunchuk.ay = accel_y >> 2;
    nunchuk.az = accel_z >> 2;

    nunchuk.bt.acc_x_lsb = accel_x & 0b11;
    nunchuk.bt.acc_y_lsb = accel_y & 0b11;
    nunchuk.bt.acc_z_lsb = accel_z & 0b11;

    nunchuk.bt.hex ^= 0b11;
    GetButton<u8>(m_c_button, nunchuk.bt.hex, WiimoteEmu::Nunchuk::BUTTON_C);
    GetButton<u8>(m_z_button, nunchuk.bt.hex, WiimoteEmu::Nunchuk::BUTTON_Z);
    nunchuk.bt.hex ^= 0b11;

    WiimoteEncrypt(&key, reinterpret_cast<u8*>(&nunchuk), 0, sizeof(wm_nc));
  }

  if (m_classic_left_stick_box->isVisible())
  {
    wm_classic_extension& cc = *reinterpret_cast<wm_classic_extension*>(ext_data);
    WiimoteDecrypt(&key, reinterpret_cast<u8*>(&cc), 0, sizeof(wm_classic_extension));

    cc.bt.hex ^= 0xFFFF;
    GetButton<u16>(m_classic_a_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_A);
    GetButton<u16>(m_classic_b_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_B);
    GetButton<u16>(m_classic_x_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_X);
    GetButton<u16>(m_classic_y_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_Y);
    GetButton<u16>(m_classic_plus_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_PLUS);
    GetButton<u16>(m_classic_minus_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_MINUS);
    GetButton<u16>(m_classic_l_button, cc.bt.hex, WiimoteEmu::Classic::TRIGGER_L);
    GetButton<u16>(m_classic_r_button, cc.bt.hex, WiimoteEmu::Classic::TRIGGER_R);
    GetButton<u16>(m_classic_zl_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_ZL);
    GetButton<u16>(m_classic_zr_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_ZR);
    GetButton<u16>(m_classic_home_button, cc.bt.hex, WiimoteEmu::Classic::BUTTON_HOME);
    GetButton<u16>(m_classic_left_button, cc.bt.hex, WiimoteEmu::Classic::PAD_LEFT);
    GetButton<u16>(m_classic_up_button, cc.bt.hex, WiimoteEmu::Classic::PAD_UP);
    GetButton<u16>(m_classic_down_button, cc.bt.hex, WiimoteEmu::Classic::PAD_DOWN);
    GetButton<u16>(m_classic_right_button, cc.bt.hex, WiimoteEmu::Classic::PAD_RIGHT);
    cc.bt.hex ^= 0xFFFF;

    u8 rx = (cc.rx1 & 0b1) & ((cc.rx2 & 0b11) << 1) & ((cc.rx3 & 0b11) << 3);
    GetSpinBoxU8(m_classic_right_stick_x_value, rx);
    cc.rx1 = rx & 0b1;
    cc.rx2 = (rx >> 1) & 0b11;
    cc.rx3 = (rx >> 3) & 0b11;

    u8 ry = cc.ry;
    GetSpinBoxU8(m_classic_right_stick_y_value, ry);
    cc.ry = ry;

    u8 lx = cc.regular_data.lx;
    GetSpinBoxU8(m_classic_left_stick_x_value, lx);
    cc.regular_data.lx = lx;

    u8 ly = cc.regular_data.ly;
    GetSpinBoxU8(m_classic_left_stick_y_value, ly);
    cc.regular_data.ly = ly;

    u8 rt = cc.rt;
    GetSpinBoxU8(m_right_trigger_value, rt);
    cc.rt = rt;

    u8 lt = (cc.lt1 & 0b111) & (cc.lt2 >> 3);
    GetSpinBoxU8(m_left_trigger_value, lt);
    cc.lt1 = lt & 0b111;
    cc.lt2 = (lt >> 3) & 0b11;

    WiimoteEncrypt(&key, reinterpret_cast<u8*>(&cc), 0, sizeof(wm_classic_extension));
  }
}
