// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include "Core/HW/WiiSave.h"
#include "Core/WiiUtils.h"
#include "DiscIO/NANDImporter.h"

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

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_utils_WiiUtils_importNANDBin(JNIEnv* env,
                                                                                   jclass,
                                                                                   jstring jFile)
{
  const std::string path = GetJString(env, jFile);

  return DiscIO::NANDImporter().ImportNANDBin(
      path,
      [] {
        // This callback gets called every now and then in case we want to update the GUI. However,
        // we have no way of knowing what the current progress is, so we can't do anything
        // especially useful. DolphinQt chooses to show the elapsed time, for reference.
      },
      [] {
        // This callback gets called if the NAND file does not have decryption keys appended to it.
        // We're supposed to ask the user for a separate file containing keys, but this is probably
        // more work to implement on Android than it's worth, as this case almost never comes up.
        PanicAlertFmtT("The decryption keys need to be appended to the NAND backup file.");
        return "";
      });
}
}
