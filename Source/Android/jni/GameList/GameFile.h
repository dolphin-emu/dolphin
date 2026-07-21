// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

#include <memory>
#include <string>

namespace UICommon
{
class GameFile;
}

jobject GameFileToJava(JNIEnv* env, std::shared_ptr<const UICommon::GameFile> game_file);
