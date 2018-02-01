// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QVBoxLayout;

class GCPadWiiUConfigDialog final : public QDialog
{
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);

private:
  void LoadSettings();
  void SaveSettings();

  void CreateLayout();
  void ConnectWidgets();

  int m_port;

  QVBoxLayout* m_layout;
  QLabel* m_status_label;
  QDialogButtonBox* m_button_box;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;
};
