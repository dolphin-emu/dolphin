// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASSpinBox.h"

#include "DolphinQt/QtUtils/QueueOnObject.h"

TASSpinBox::TASSpinBox(QWidget* parent) : QSpinBox(parent)
{
  connect(this, QOverload<int>::of(&TASSpinBox::valueChanged), this, &TASSpinBox::OnUIValueChanged);
}

int TASSpinBox::GetValue() const
{
  return m_state.GetValue();
}

void TASSpinBox::OnControllerValueChanged(int new_value)
{
  if (m_state.OnControllerValueChanged(static_cast<u16>(new_value)))
    QueueOnObject(this, &TASSpinBox::ApplyControllerValueChange);
}

void TASSpinBox::OnUIValueChanged(int new_value)
{
  m_state.OnUIValueChanged(static_cast<u16>(new_value));
}

void TASSpinBox::ApplyControllerValueChange()
{
  const QSignalBlocker blocker(this);
  setValue(m_state.ApplyControllerValueChange());
}
