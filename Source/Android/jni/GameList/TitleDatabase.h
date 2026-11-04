// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

namespace Core
{
class TitleDatabase;
}

Core::TitleDatabase* TitleDatabaseFromJava(JNIEnv* env, jobject obj);
