// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Host.h"

#include <mutex>

std::mutex HostThreadLock::s_host_identity_mutex;
