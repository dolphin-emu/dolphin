// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMenu>

namespace QtUtils
{

// A menu widget based on QMenu that will not be automatically dismissed when one of its checkable
// actions are triggered.
class NonAutodismissibleMenu : public QMenu
{
public:
  using QMenu::QMenu;

protected:
  void mouseReleaseEvent(QMouseEvent* event) override;
};

}  // namespace QtUtils
