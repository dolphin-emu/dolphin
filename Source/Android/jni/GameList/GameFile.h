// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <jni.h>

namespace UICommon
{
class GameFile;
}

jobject GameFileToJava(JNIEnv* env, std::shared_ptr<const UICommon::GameFile> game_file);
