// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class QCheckBox;
class QSpinBox;
struct GCPadStatus;

class GCTASInputWindow : public QDialog
{
  Q_OBJECT
public:
  explicit GCTASInputWindow(QWidget* parent, int num);
  void GetValues(GCPadStatus* pad);

private:
  QCheckBox* m_a_button;
  QCheckBox* m_b_button;
  QCheckBox* m_x_button;
  QCheckBox* m_y_button;
  QCheckBox* m_z_button;
  QCheckBox* m_l_button;
  QCheckBox* m_r_button;
  QCheckBox* m_start_button;
  QCheckBox* m_left_button;
  QCheckBox* m_up_button;
  QCheckBox* m_down_button;
  QCheckBox* m_right_button;
  QSpinBox* m_l_trigger_value;
  QSpinBox* m_r_trigger_value;
  QSpinBox* m_x_main_stick_value;
  QSpinBox* m_y_main_stick_value;
  QSpinBox* m_x_c_stick_value;
  QSpinBox* m_y_c_stick_value;
};
