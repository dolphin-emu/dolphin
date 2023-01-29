// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Graphics/GraphicsColor.h"

#include <QColorDialog>
#include <QString>

GraphicsColor::GraphicsColor(QWidget* parent, bool supports_alpha)
    : QPushButton(parent), m_supports_alpha(supports_alpha)
{
  connect(this, SIGNAL(clicked()), this, SLOT(ChangeColor()));
}

void GraphicsColor::UpdateColor()
{
  setStyleSheet(QStringLiteral("background-color: ") + m_color.name());
  emit ColorIsUpdated();
}

void GraphicsColor::ChangeColor()
{
  QColor newColor;
  if (m_supports_alpha)
  {
    newColor = QColorDialog::getColor(m_color, parentWidget(), QStringLiteral("Pick a color"),
                                      QColorDialog::ShowAlphaChannel);
  }
  else
  {
    newColor = QColorDialog::getColor(m_color, parentWidget(), QStringLiteral("Pick a color"),
                                      QColorDialog::ShowAlphaChannel);
  }
  if (newColor != m_color)
  {
    SetColor(newColor);
  }
}

void GraphicsColor::SetColor(const QColor& color)
{
  m_color = color;
  UpdateColor();
}

const QColor& GraphicsColor::GetColor()
{
  return m_color;
}
