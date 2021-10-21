// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipRadioButton.h"

#include <QStyle>
#include <QStyleOption>

ToolTipRadioButton::ToolTipRadioButton(const QString& label) : ToolTipWidget(label)
{
  SetTitle(label);
}

QPoint ToolTipRadioButton::GetToolTipPosition() const
{
  int radio_button_width = 18;
  if (style())
  {
    QStyleOptionButton opt;
    initStyleOption(&opt);
    radio_button_width =
        style()->subElementRect(QStyle::SubElement::SE_RadioButtonIndicator, &opt, this).width();
  }

  return pos() + QPoint(radio_button_width / 2, height() / 2);
}
