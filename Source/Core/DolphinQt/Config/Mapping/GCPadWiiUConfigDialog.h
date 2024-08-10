// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QVBoxLayout;

class GCPadWiiUConfigDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);
  ~GCPadWiiUConfigDialog();

private:
  void LoadSettings() const;
  void SaveSettings() const;

  void CreateLayout();
  void ConnectWidgets();

private:
  void UpdateAdapterStatus() const;

  int m_port;

  QVBoxLayout* m_layout;
  QLabel* m_status_label;
  QDialogButtonBox* m_button_box;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;
};
