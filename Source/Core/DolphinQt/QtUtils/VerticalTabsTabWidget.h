// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTabWidget>
#include <QWidget>

namespace QtUtils
{

// A tab widget where tabs are drawn to the left of the pages with horizontal text.
class VerticalTabsTabWidget : public QTabWidget
{
public:
  explicit VerticalTabsTabWidget(QWidget* parent = nullptr);
};

}  // namespace QtUtils
