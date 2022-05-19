// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace EMM
{
void InstallExceptionHandler();
void UninstallExceptionHandler();
bool IsExceptionHandlerSupported();
}  // namespace EMM
