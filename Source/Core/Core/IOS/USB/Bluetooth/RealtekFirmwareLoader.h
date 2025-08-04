// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

class LibUSBBluetoothAdapter;

bool IsRealtekVID(u16 vid);
bool IsKnownRealtekBluetoothDevice(u16 vid, u16 pid);

// Identifies the device and loads firmware as needed.
bool InitializeRealtekBluetoothDevice(LibUSBBluetoothAdapter&);
