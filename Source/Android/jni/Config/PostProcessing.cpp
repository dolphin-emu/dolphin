// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "VideoCommon/PostProcessing.h"
#include "jni/AndroidCommon/AndroidCommon.h"

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getShaderList(JNIEnv* env,
                                                                                    jclass)
{
  return VectorToJStringArray(env, VideoCommon::PostProcessing::GetShaderList());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getAnaglyphShaderList(
    JNIEnv* env, jclass)
{
  return VectorToJStringArray(env, VideoCommon::PostProcessing::GetAnaglyphShaderList());
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getPassiveShaderList(
    JNIEnv* env, jclass)
{
  return VectorToJStringArray(env, VideoCommon::PostProcessing::GetPassiveShaderList());
}
}
