// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ClearLayoutRecursively.h"

#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

void ClearLayoutRecursively(QLayout* layout)
{
  while (QLayoutItem* child = layout->takeAt(0))
  {
    if (child == nullptr)
      continue;

    if (child->widget())
    {
      layout->removeWidget(child->widget());
      delete child->widget();
    }
    else if (child->layout())
    {
      ClearLayoutRecursively(child->layout());
      layout->removeItem(child);
    }
    else
    {
      layout->removeItem(child);
    }
    delete child;
  }
}
