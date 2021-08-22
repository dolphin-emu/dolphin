// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/PartiallyClosableTabWidget.h"

#include <QStyle>
#include <QTabBar>

PartiallyClosableTabWidget::PartiallyClosableTabWidget(QWidget* parent) : QTabWidget(parent)
{
  setTabsClosable(true);
}

void PartiallyClosableTabWidget::setTabUnclosable(int index)
{
  QTabBar::ButtonPosition closeSide = (QTabBar::ButtonPosition)style()->styleHint(
      QStyle::SH_TabBar_CloseButtonPosition, nullptr, this);
  tabBar()->setTabButton(index, closeSide, nullptr);
}
