// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QGridLayout;
class QLabel;

class DualShockUDPClientCalibrationDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit DualShockUDPClientCalibrationDialog(QWidget* parent, int server_index);

private:
  void CreateWidgets();

  void UpdateCalibrationValues();
  void CommitCalibration();

  QDialogButtonBox* m_button_box;
  QPushButton* m_confirm_button;
  QGridLayout* m_main_layout;
  QLabel* m_min_x;
  QLabel* m_min_y;
  QLabel* m_max_x;
  QLabel* m_max_y;
  QLabel* m_device_state;

  const int m_server_index;
};
