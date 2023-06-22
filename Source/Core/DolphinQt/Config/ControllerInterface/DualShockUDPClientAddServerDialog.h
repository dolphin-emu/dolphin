// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QGridLayout;
class QLineEdit;
class QSpinBox;

class DualShockUDPClientAddServerDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit DualShockUDPClientAddServerDialog(QWidget* parent);

private:
  void CreateWidgets();
  void OnServerAdded();

  QDialogButtonBox* m_buttonbox;
  QGridLayout* m_main_layout;
  QLineEdit* m_description;
  QLineEdit* m_server_address;
  QSpinBox* m_server_port;
};
