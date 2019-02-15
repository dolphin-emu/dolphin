// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <jni.h>

namespace IDCache
{
static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

JavaVM* GetJavaVM();

jclass GetNativeLibraryClass();
jmethodID GetDisplayAlertMsg();

jclass GetAnalyticsClass();
jmethodID GetSendAnalyticsReport();
jmethodID GetAnalyticsValue();

jclass GetGameFileClass();
jfieldID GetGameFilePointer();
jmethodID GetGameFileConstructor();

jclass GetGameFileCacheClass();
jfieldID GetGameFileCachePointer();

}  // namespace IDCache
