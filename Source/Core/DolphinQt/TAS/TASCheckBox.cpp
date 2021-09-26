// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASCheckBox.h"

#include <QMouseEvent>

#include "Core/Movie.h"
#include "DolphinQt/TAS/TASInputWindow.h"

TASCheckBox::TASCheckBox(const QString& text, TASInputWindow* parent)
    : QCheckBox(text, parent), m_parent(parent)
{
  setTristate(true);
}

bool TASCheckBox::GetValue() const
{
  if (checkState() == Qt::PartiallyChecked)
  {
    const u64 frames_elapsed = Movie::GetCurrentFrame() - m_frame_turbo_started;
    return static_cast<int>(frames_elapsed % m_turbo_total_frames) < m_turbo_press_frames;
  }

  return isChecked();
}

void TASCheckBox::mousePressEvent(QMouseEvent* event)
{
  if (event->button() != Qt::RightButton)
  {
    setChecked(!isChecked());
    setCheckboxWeight(QFont::Weight::Normal);

    return;
  }

  if (checkState() == Qt::PartiallyChecked)
  {
    setCheckState(Qt::Unchecked);
    setCheckboxWeight(QFont::Weight::Normal);

    return;
  }

  setCheckboxWeight(QFont::Weight::Bold);

  m_frame_turbo_started = Movie::GetCurrentFrame();
  m_turbo_press_frames = m_parent->GetTurboPressFrames();
  m_turbo_total_frames = m_turbo_press_frames + m_parent->GetTurboReleaseFrames();
  setCheckState(Qt::PartiallyChecked);
}

void TASCheckBox::setCheckboxWeight(QFont::Weight weight)
{
  QFont font = this->font();
  font.setWeight(weight);
  this->setFont(font);
}
