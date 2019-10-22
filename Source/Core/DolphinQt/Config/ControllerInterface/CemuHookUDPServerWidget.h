// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QSpinBox;

class CemuHookUDPServerWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit CemuHookUDPServerWidget();

private:
  void CreateWidgets();
  void ConnectWidgets();

  QCheckBox* m_server_enabled;
  QLineEdit* m_server_address;
  QSpinBox* m_server_port;
};
