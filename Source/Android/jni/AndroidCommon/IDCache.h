// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <jni.h>

namespace IDCache
{
JNIEnv* GetEnvForThread();

jclass GetNativeLibraryClass();
jmethodID GetDisplayAlertMsg();
jmethodID GetDoRumble();
jmethodID GetUpdateTouchPointer();
jmethodID GetOnTitleChanged();
jmethodID GetFinishEmulationActivity();

jclass GetAnalyticsClass();
jmethodID GetSendAnalyticsReport();
jmethodID GetAnalyticsValue();

jclass GetGameFileClass();
jfieldID GetGameFilePointer();
jmethodID GetGameFileConstructor();

jclass GetGameFileCacheClass();
jfieldID GetGameFileCachePointer();

jclass GetLinkedHashMapClass();
jmethodID GetLinkedHashMapInit();
jmethodID GetLinkedHashMapPut();

jclass GetIniFileClass();
jfieldID GetIniFilePointer();
jclass GetIniFileSectionClass();
jfieldID GetIniFileSectionPointer();
jmethodID GetIniFileSectionConstructor();

jclass GetCompressCallbackClass();
jmethodID GetCompressCallbackRun();

jclass GetContentHandlerClass();
jmethodID GetContentHandlerOpenFd();
jmethodID GetContentHandlerDelete();

jclass GetNetworkHelperClass();
jmethodID GetNetworkHelperGetNetworkIpAddress();
jmethodID GetNetworkHelperGetNetworkPrefixLength();
jmethodID GetNetworkHelperGetNetworkGateway();

}  // namespace IDCache
