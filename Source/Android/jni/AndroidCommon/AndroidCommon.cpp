// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/AndroidCommon.h"

#include <string>
#include <vector>

#include <fmt/format.h>

#include <jni.h>

#include "Common/StringUtil.h"
#include "jni/AndroidCommon/IDCache.h"

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
    result.push_back(fmt::to_string((jstring)env->GetObjectArrayElement(array, i)));

  return result;
}

int OpenAndroidContent(const std::string& uri, const std::string& mode)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  const jint fd = env->CallStaticIntMethod(IDCache::GetContentHandlerClass(),
                                           IDCache::GetContentHandlerOpenFd(), ToJString(env, uri),
                                           ToJString(env, mode));

  // We can get an IllegalArgumentException when passing an invalid mode
  if (env->ExceptionCheck())
  {
    env->ExceptionDescribe();
    abort();
  }

  return fd;
}

bool DeleteAndroidContent(const std::string& uri)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticBooleanMethod(IDCache::GetContentHandlerClass(),
                                      IDCache::GetContentHandlerDelete(), ToJString(env, uri));
}
