// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASCheckBox.h"

#include <QMouseEvent>

#include "Core/Movie.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/TAS/TASInputWindow.h"

TASCheckBox::TASCheckBox(const QString& text, TASInputWindow* parent)
    : QCheckBox(text, parent), m_parent(parent)
{
  m_is_turbo = false;
}

bool TASCheckBox::GetValue()
{
  if (m_is_turbo)
  {
    const u64 frames_elapsed = Movie::GetCurrentFrame() - m_frame_turbo_started;
    const u64 frame_in_sequence = frames_elapsed % m_turbo_total_frames;
    m_state_changed = false;

    if ((frame_in_sequence + 1 == m_turbo_press_frames) ||
        (frame_in_sequence + 1 == m_turbo_total_frames))
      m_state_changed = true;

    return static_cast<int>(frame_in_sequence) < m_turbo_press_frames;
  }

  return isChecked();
}

void TASCheckBox::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::RightButton)
  {
    setChecked(!isChecked());
    setCheckboxWeight(QFont::Weight::Normal);
    m_is_turbo = false;

    return;
  }

  if (checkState() == Qt::PartiallyChecked)
  {
    setCheckState(Qt::Unchecked);
    setCheckboxWeight(QFont::Weight::Normal);
    m_is_turbo = false;

    return;
  }

  setCheckboxWeight(QFont::Weight::Bold);

  m_frame_turbo_started = Movie::GetCurrentFrame();
  m_turbo_press_frames = m_parent->GetTurboPressFrames();
  m_turbo_total_frames = m_turbo_press_frames + m_parent->GetTurboReleaseFrames();
  m_is_turbo = true;
  setCheckState(Qt::Checked);
}

void TASCheckBox::setCheckboxWeight(QFont::Weight weight)
{
  QFont font = this->font();
  font.setWeight(weight);
  this->setFont(font);
}

bool TASCheckBox::GetIsTurbo()
{
  return m_is_turbo;
}

bool TASCheckBox::GetStateChanged()
{
  return m_state_changed;
}
