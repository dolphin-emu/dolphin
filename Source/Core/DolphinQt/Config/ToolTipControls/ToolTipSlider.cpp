// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ToolTipControls/ToolTipSlider.h"

#include <QStyle>
#include <QStyleOption>

ToolTipSlider::ToolTipSlider(Qt::Orientation orientation) : ToolTipWidget(orientation)
{
}

QPoint ToolTipSlider::GetToolTipPosition() const
{
  QRect handle_rect(0, 0, 15, 15);
  if (style())
  {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    handle_rect = style()->subControlRect(QStyle::ComplexControl::CC_Slider, &opt,
                                          QStyle::SubControl::SC_SliderHandle, this);
  }

  return pos() + handle_rect.center();
}
