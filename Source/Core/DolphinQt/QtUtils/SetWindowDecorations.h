// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QWidget;

// Changes the window decorations (title bar) to dark if the user uses dark mode on Windows.
void SetQWidgetWindowDecorations(QWidget* widget);
