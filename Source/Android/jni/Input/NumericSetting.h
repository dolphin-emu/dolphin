// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

namespace ControllerEmu
{
class NumericSettingBase;
}

class ControlReference;

jobject NumericSettingToJava(JNIEnv* env, ControllerEmu::NumericSettingBase* control);
