// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <jni.h>

namespace IDCache
{
JavaVM* GetJavaVM();

jclass GetNativeLibraryClass();
jmethodID GetDisplayAlertMsg();
jmethodID GetRumbleOutputMethod();

jclass GetGameFileClass();
jfieldID GetGameFilePointer();
jmethodID GetGameFileConstructor();

jclass GetGameFileCacheClass();
jfieldID GetGameFileCachePointer();

}  // namespace IDCache
