// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

#include <memory>

namespace ciface::Core
{
class Device;
}

jobject CoreDeviceToJava(JNIEnv* env, std::shared_ptr<ciface::Core::Device> device);
