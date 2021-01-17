// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <jni.h>

#include "jni/AndroidCommon/AndroidCommon.h"

#include "Core/WiiUtils.h"

extern "C" {

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_installWAD(JNIEnv* env,
                                                                                    jclass,
                                                                                    jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  return static_cast<jboolean>(WiiUtils::InstallWAD(path));
}
}
