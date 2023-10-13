// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QVBoxLayout;
class QTimer;

class GCPadWiiUConfigDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);
  ~GCPadWiiUConfigDialog();

private:
  void LoadSettings();
  void SaveSettings();

  void CreateLayout();
  void ConnectWidgets();

private:
  void UpdateAdapterStatus();
  void UpdatePollRate();

  int m_port;

  QVBoxLayout* m_layout;
  QLabel* m_status_label;
  QLabel* m_poll_rate_label;
  QTimer* m_poll_rate_timer;
  QDialogButtonBox* m_button_box;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;
};
