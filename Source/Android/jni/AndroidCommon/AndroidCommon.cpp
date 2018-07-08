// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/AndroidCommon.h"

#include <string>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr)
{
  std::string result = "";
  if (!jstr)
    return result;

  const char* s = env->GetStringUTFChars(jstr, nullptr);
  result = s;
  env->ReleaseStringUTFChars(jstr, s);
  return result;
}

jstring ToJString(JNIEnv* env, const std::string& str)
{
  return env->NewStringUTF(str.c_str());
}
