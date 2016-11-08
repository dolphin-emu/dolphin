// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Intended for containing event functions that are bound to
// different window controls through wxEvtHandler's Bind()
// function.

class wxUpdateUIEvent;

namespace WxEventUtils
{
void OnEnableIfCoreInitialized(wxUpdateUIEvent&);
void OnEnableIfCoreUninitialized(wxUpdateUIEvent&);
void OnEnableIfCoreRunning(wxUpdateUIEvent&);
void OnEnableIfCoreNotRunning(wxUpdateUIEvent&);
void OnEnableIfCorePaused(wxUpdateUIEvent&);
void OnEnableIfCoreRunningOrPaused(wxUpdateUIEvent&);

void OnEnableIfCPUCanStep(wxUpdateUIEvent&);

void OnEnableIfNetplayNotRunning(wxUpdateUIEvent&);
}  // namespace WxEventUtils
