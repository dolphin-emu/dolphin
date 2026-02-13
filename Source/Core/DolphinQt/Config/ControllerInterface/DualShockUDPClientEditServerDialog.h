// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QGridLayout;
class QLineEdit;
class QSpinBox;

class DualShockUDPClientEditServerDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit DualShockUDPClientEditServerDialog(QWidget* parent, std::optional<size_t> existing_index);

private:
  void CreateWidgets();
  void OnServerFinished();

  std::optional<size_t> m_existing_index;
  QDialogButtonBox* m_buttonbox;
  QGridLayout* m_main_layout;
  QLineEdit* m_description;
  QLineEdit* m_server_address;
  QSpinBox* m_server_port;
};
