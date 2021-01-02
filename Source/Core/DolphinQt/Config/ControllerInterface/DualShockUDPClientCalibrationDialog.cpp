// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ControllerInterface/DualShockUDPClientCalibrationDialog.h"

#include <fmt/format.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "Common/Config/Config.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

DualShockUDPClientCalibrationDialog::DualShockUDPClientCalibrationDialog(QWidget* parent,
                                                                         int server_index)
    : QDialog(parent), m_server_index(server_index)
{
  CreateWidgets();
  setLayout(m_main_layout);
}

void DualShockUDPClientCalibrationDialog::CreateWidgets()
{
  setWindowTitle(tr("Calibrate Touch"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_main_layout = new QGridLayout;
  m_main_layout->setSizeConstraint(QLayout::SetFixedSize);

  m_min_x = new QLabel;
  m_min_y = new QLabel;
  m_max_x = new QLabel;
  m_max_y = new QLabel;
  m_device_state = new QLabel;

  m_main_layout->addWidget(
      new QLabel(tr("Touch the surface around the top left and the bottom "
                    "right\nuntil values don't change anymore.\nDS4 and DualSense are already "
                    "calibrated.\nThe same calibration will apply to all the devices\nconnected "
                    "from this server.")),
      0, 0, 1, 2);
  m_main_layout->addWidget(new QLabel(tr("Min X:")), 1, 0);
  m_main_layout->addWidget(m_min_x, 1, 1);
  m_main_layout->addWidget(new QLabel(tr("Min Y:")), 2, 0);
  m_main_layout->addWidget(m_min_y, 2, 1);
  m_main_layout->addWidget(new QLabel(tr("Max X:")), 3, 0);
  m_main_layout->addWidget(m_max_x, 3, 1);
  m_main_layout->addWidget(new QLabel(tr("Max Y:")), 4, 0);
  m_main_layout->addWidget(m_max_y, 4, 1);
  m_main_layout->addWidget(new QLabel(tr("Device State:")), 5, 0);
  m_main_layout->addWidget(m_device_state, 5, 1);

  m_button_box = new QDialogButtonBox();
  m_confirm_button = new QPushButton(tr("Confirm"));
  auto* cancel_button = new QPushButton(tr("Cancel"));
  m_button_box->addButton(m_confirm_button, QDialogButtonBox::AcceptRole);
  m_button_box->addButton(cancel_button, QDialogButtonBox::RejectRole);
  connect(m_confirm_button, &QPushButton::clicked, this,
          &DualShockUDPClientCalibrationDialog::CommitCalibration);
  connect(cancel_button, &QPushButton::clicked, this, &DualShockUDPClientCalibrationDialog::reject);
  m_confirm_button->setDefault(true);

  m_main_layout->addWidget(m_button_box, 6, 0, 1, 2);

  const auto timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this,
          &DualShockUDPClientCalibrationDialog::UpdateCalibrationValues);
  UpdateCalibrationValues();
  timer->start(1000 / 60);  // ~60Hz
}

void DualShockUDPClientCalibrationDialog::UpdateCalibrationValues()
{
  QString x_min = tr("None");
  QString y_min = x_min;
  QString x_max = x_min;
  QString y_max = x_min;
  // g_calibration_device_found won't be set back to false if the device is detached
  // because Dolphin isn't even aware of that, and it's not a problem anyway
  if (ciface::DualShockUDPClient::g_calibration_device_found)
  {
    const bool x_calibration_valid = ciface::DualShockUDPClient::g_calibration_touch_x_max >
                                     ciface::DualShockUDPClient::g_calibration_touch_x_min;
    const bool y_calibration_valid = ciface::DualShockUDPClient::g_calibration_touch_y_max >
                                     ciface::DualShockUDPClient::g_calibration_touch_y_min;
    const bool calibration_valid = x_calibration_valid && y_calibration_valid;
    if (x_calibration_valid)
    {
      x_min = QStringLiteral("%1").arg(ciface::DualShockUDPClient::g_calibration_touch_x_min);
      x_max = QStringLiteral("%1").arg(ciface::DualShockUDPClient::g_calibration_touch_x_max);
    }
    if (y_calibration_valid)
    {
      y_min = QStringLiteral("%1").arg(ciface::DualShockUDPClient::g_calibration_touch_y_min);
      y_max = QStringLiteral("%1").arg(ciface::DualShockUDPClient::g_calibration_touch_y_max);
    }

    m_device_state->setText(calibration_valid ? tr("Found. Calibration Valid") :
                                                tr("Found. Calibration Invalid"));
    m_confirm_button->setEnabled(calibration_valid);
  }
  else
  {
    m_device_state->setText(tr("Not Found"));
    m_confirm_button->setEnabled(false);
  }

  m_min_x->setText(x_min);
  m_min_y->setText(y_min);
  m_max_x->setText(x_max);
  m_max_y->setText(y_max);
}

void DualShockUDPClientCalibrationDialog::CommitCalibration()
{
  const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
  auto server_details = SplitString(servers_setting, ';');
  std::string new_server_setting;

  for (size_t i = 0; i < server_details.size(); i++)
  {
    if (i == m_server_index)
    {
      std::string new_server_info;
      auto server_info = SplitString(server_details[i], ':');
      constexpr int CALIBRATION_INDEX = 4;
      // If we don't have index 3 and 4, add them now as empty, which is accepted
      while (server_info.size() <= CALIBRATION_INDEX)
      {
        server_info.emplace_back();
      }
      for (size_t k = 0; k < server_info.size(); k++)
      {
        if (k == CALIBRATION_INDEX)
        {
          server_info[k] = fmt::format("({},{},{},{})", m_min_x->text().toStdString(),
                                       m_min_y->text().toStdString(), m_max_x->text().toStdString(),
                                       m_max_y->text().toStdString());
        }
        new_server_info += server_info[k];
        if (k + 1 != server_info.size())
        {
          new_server_info += ':';
        }
      }
      server_details[i] = new_server_info;
    }

    new_server_setting += server_details[i] + ';';
  }

  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS, new_server_setting);

  accept();
}
