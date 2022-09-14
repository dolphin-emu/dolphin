// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

namespace IDCache
{
JNIEnv* GetEnvForThread();

jclass GetStringClass();

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
jmethodID GetContentHandlerGetSizeAndIsDirectory();
jmethodID GetContentHandlerGetDisplayName();
jmethodID GetContentHandlerGetChildNames();
jmethodID GetContentHandlerDoFileSearch();

jclass GetNetworkHelperClass();
jmethodID GetNetworkHelperGetNetworkIpAddress();
jmethodID GetNetworkHelperGetNetworkPrefixLength();
jmethodID GetNetworkHelperGetNetworkGateway();

jmethodID GetBooleanSupplierGet();

jclass GetARCheatClass();
jfieldID GetARCheatPointer();
jmethodID GetARCheatConstructor();

jclass GetGeckoCheatClass();
jfieldID GetGeckoCheatPointer();
jmethodID GetGeckoCheatConstructor();

jclass GetPatchCheatClass();
jfieldID GetPatchCheatPointer();
jmethodID GetPatchCheatConstructor();

jclass GetGraphicsModGroupClass();
jfieldID GetGraphicsModGroupPointer();
jmethodID GetGraphicsModGroupConstructor();

jclass GetGraphicsModClass();
jfieldID GetGraphicsModPointer();
jmethodID GetGraphicsModConstructor();

jclass GetRiivolutionPatchesClass();
jfieldID GetRiivolutionPatchesPointer();

jclass GetWiiUpdateCallbackClass();
jmethodID GetWiiUpdateCallbackFunction();

}  // namespace IDCache
