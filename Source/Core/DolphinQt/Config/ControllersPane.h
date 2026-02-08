// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class WiimoteControllersWidget;

class ControllersPane final : public QWidget
{
  Q_OBJECT
public:
  ControllersPane();

protected:
  void showEvent(QShowEvent* event) override;

private:
  void CreateMainLayout();

  WiimoteControllersWidget* m_wiimote_controllers;
};
