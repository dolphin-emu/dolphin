// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Graphics/GraphicsWidget.h"

#include <QEvent>
#include <QLabel>

#include "DolphinQt2/Config/Graphics/GraphicsWindow.h"

GraphicsWidget::GraphicsWidget(GraphicsWindow* parent)
{
  parent->RegisterWidget(this);
}

void GraphicsWidget::AddDescription(QWidget* widget, const char* description)
{
  emit DescriptionAdded(widget, description);
}
