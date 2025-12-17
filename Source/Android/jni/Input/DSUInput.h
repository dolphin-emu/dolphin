// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Enable/disable background input execution. When enabled a background thread
// may be created to poll controller input; when disabled the thread will be
// stopped and joined so it doesn't prevent the CPU from sleeping.
void SetBackgroundInputExecutionAllowed(bool allowed);
