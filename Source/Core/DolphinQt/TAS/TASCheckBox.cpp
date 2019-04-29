// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/TAS/TASCheckBox.h"

#include <QMouseEvent>

#include "Core/Movie.h"

TASCheckBox::TASCheckBox(const QString& text) : QCheckBox(text)
{
  setTristate(true);
}

bool TASCheckBox::GetValue()
{
  if (checkState() == Qt::PartiallyChecked)
    return Movie::GetCurrentFrame() % 2 == static_cast<u64>(m_trigger_on_odd);

  return isChecked();
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

  m_trigger_on_odd = Movie::GetCurrentFrame() % 2 == 0;
  setCheckState(Qt::PartiallyChecked);
}
