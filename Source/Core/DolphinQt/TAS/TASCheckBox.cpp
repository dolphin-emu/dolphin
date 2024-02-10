// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASCheckBox.h"

#include <QMouseEvent>

#include "Core/Movie.h"
#include "Core/System.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/TAS/TASInputWindow.h"

TASCheckBox::TASCheckBox(const QString& text, TASInputWindow* parent)
    : QCheckBox(text, parent), m_parent(parent)
{
  setTristate(true);

  connect(this, &TASCheckBox::stateChanged, this, &TASCheckBox::OnUIValueChanged);
}

bool TASCheckBox::GetValue() const
{
  Qt::CheckState check_state = static_cast<Qt::CheckState>(m_state.GetValue());

  if (check_state == Qt::PartiallyChecked)
  {
    const u64 frames_elapsed =
        Core::System::GetInstance().GetMovie().GetCurrentFrame() - m_frame_turbo_started;
    return static_cast<int>(frames_elapsed % m_turbo_total_frames) < m_turbo_press_frames;
  }

  return check_state != Qt::Unchecked;
}

void TASCheckBox::OnControllerValueChanged(bool new_value)
{
  if (m_state.OnControllerValueChanged(new_value ? Qt::Checked : Qt::Unchecked))
    QueueOnObject(this, &TASCheckBox::ApplyControllerValueChange);
}

void TASCheckBox::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::RightButton)
  {
    setChecked(!isChecked());
    return;
  }

  if (checkState() == Qt::PartiallyChecked)
  {
    setCheckState(Qt::Unchecked);
    return;
  }

  m_frame_turbo_started = Core::System::GetInstance().GetMovie().GetCurrentFrame();
  m_turbo_press_frames = m_parent->GetTurboPressFrames();
  m_turbo_total_frames = m_turbo_press_frames + m_parent->GetTurboReleaseFrames();
  setCheckState(Qt::PartiallyChecked);
}

void TASCheckBox::OnUIValueChanged(int new_value)
{
  m_state.OnUIValueChanged(new_value);
}

void TASCheckBox::ApplyControllerValueChange()
{
  const QSignalBlocker blocker(this);
  setCheckState(static_cast<Qt::CheckState>(m_state.ApplyControllerValueChange()));
}
