// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;

class ResetSettingsDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit ResetSettingsDialog(QWidget* parent);

private:
  void ResetSelected();

  QCheckBox* m_reset_core;
  QCheckBox* m_reset_graphics;
  QCheckBox* m_reset_paths;
  QCheckBox* m_reset_gui;
  QCheckBox* m_reset_controller;
  QCheckBox* m_reset_netplay;
};
