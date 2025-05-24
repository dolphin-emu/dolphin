// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(_WIN32)

class QApplication;

namespace QtUtils
{
// Changes the window decorations (title bar) for Windows "Dark" mode or "Dark" Dolphin Style.
void InstallWindowDecorationFilter(QApplication*);
}  // namespace QtUtils

#endif
