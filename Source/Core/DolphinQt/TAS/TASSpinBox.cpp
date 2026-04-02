// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASSpinBox.h"

#include "DolphinQt/QtUtils/QueueOnObject.h"

TASSpinBox::TASSpinBox(QWidget* parent) : QSpinBox(parent)
{
  connect(this, &TASSpinBox::valueChanged, this, &TASSpinBox::OnUIValueChanged);
}

int TASSpinBox::GetValue() const
{
  return m_state.GetValue();
}

void TASSpinBox::OnControllerValueChanged(int new_value)
{
  if (m_state.OnControllerValueChanged(new_value))
    QueueOnObject(this, &TASSpinBox::ApplyControllerValueChange);
}

void TASSpinBox::OnUIValueChanged(int new_value)
{
  m_state.OnUIValueChanged(new_value);
}

void TASSpinBox::ApplyControllerValueChange()
{
  setValue(m_state.ApplyControllerValueChange());
}

QValidator::State TASSpinBox::validate(QString& input, int& pos) const
{
  auto state = QSpinBox::validate(input, pos);
  if (state == QValidator::Invalid)
  {
    bool ok;
    input.toInt(&ok);
    if (ok)
      return QValidator::Intermediate;
  }
  return state;
}

void TASSpinBox::fixup(QString& input) const
{
  bool ok;
  int val = input.toInt(&ok);
  if (ok)
  {
    if (val > maximum())
      input = QString::number(maximum());
    else if (val < minimum())
      input = QString::number(minimum());
    return;
  }
  QSpinBox::fixup(input);
}
