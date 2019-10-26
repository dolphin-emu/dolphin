// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QSpinBox;

class DualShockUDPClientWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit DualShockUDPClientWidget();

private:
  void CreateWidgets();
  void ConnectWidgets();

  QCheckBox* m_server_enabled;
  QLineEdit* m_server_address;
  QSpinBox* m_server_port;
};
