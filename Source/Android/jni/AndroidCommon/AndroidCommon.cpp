// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/AndroidCommon.h"

#include <string>
#include <string_view>
#include <vector>

#include <jni.h>

#include "Common/StringUtil.h"

std::string GetJString(JNIEnv* env, jstring jstr)
{
  const jchar* jchars = env->GetStringChars(jstr, nullptr);
  const jsize length = env->GetStringLength(jstr);
  const std::u16string_view string_view(reinterpret_cast<const char16_t*>(jchars), length);
  const std::string converted_string = UTF16ToUTF8(string_view);
  env->ReleaseStringChars(jstr, jchars);
  return converted_string;
}

jstring ToJString(JNIEnv* env, const std::string& str)
{
  const std::u16string converted_string = UTF8ToUTF16(str);
  return env->NewString(reinterpret_cast<const jchar*>(converted_string.data()),
                        converted_string.size());
}

std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray array)
{
  const jsize size = env->GetArrayLength(array);
  std::vector<std::string> result;
  result.reserve(size);

  for (jsize i = 0; i < size; ++i)
    result.push_back(GetJString(env, (jstring)env->GetObjectArrayElement(array, i)));

  return result;
}
