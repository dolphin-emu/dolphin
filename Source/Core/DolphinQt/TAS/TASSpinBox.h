// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/TAS/TASControlState.h"

#include <QSpinBox>

class TASInputWindow;

class TASSpinBox : public QSpinBox
{
  Q_OBJECT
public:
  explicit TASSpinBox(QWidget* parent = nullptr);

  // Can be called from the CPU thread
  int GetValue() const;
  // Must be called from the CPU thread
  void OnControllerValueChanged(int new_value);
  QValidator::State validate(QString& input, int& pos) const override;
  void fixup(QString& input) const override;

private slots:
  void OnUIValueChanged(int new_value);
  void ApplyControllerValueChange();

private:
  TASControlState m_state;
};
