// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <jni.h>

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include "Core/HW/WiiSave.h"
#include "Core/WiiUtils.h"

// The hardcoded values here must match WiiUtils.java
static jint ConvertCopyResult(WiiSave::CopyResult result)
{
  switch (result)
  {
  case WiiSave::CopyResult::Success:
    return 0;
  case WiiSave::CopyResult::Error:
    return 1;
  case WiiSave::CopyResult::Cancelled:
    return 2;
  case WiiSave::CopyResult::CorruptedSource:
    return 3;
  case WiiSave::CopyResult::TitleMissing:
    return 4;
  default:
    ASSERT(false);
    return 1;
  }

  static_assert(static_cast<int>(WiiSave::CopyResult::NumberOfEntries) == 5);
}

extern "C" {

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_installWAD(JNIEnv* env,
                                                                                    jclass,
                                                                                    jstring jFile)
{
  const std::string path = GetJString(env, jFile);
  return static_cast<jboolean>(WiiUtils::InstallWAD(path));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_importWiiSave(
    JNIEnv* env, jclass, jstring jFile, jobject jCanOverwrite)
{
  const std::string path = GetJString(env, jFile);
  const auto can_overwrite = [&] {
    const jmethodID get = IDCache::GetBooleanSupplierGet();
    return static_cast<bool>(env->CallBooleanMethod(jCanOverwrite, get));
  };

  return ConvertCopyResult(WiiSave::Import(path, can_overwrite));
}
}
