// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class GraphicsModWarningWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GraphicsModWarningWidget(QWidget* parent);

signals:
  void GraphicsModEnableSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Update();

  QLabel* m_text;
  QPushButton* m_config_button;
};
