// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/AndroidCommon.h"

#include <ios>
#include <string>
#include <string_view>
#include <vector>

#include <jni.h>

#include "Common/Assert.h"
#include "Common/StringUtil.h"
#include "jni/AndroidCommon/IDCache.h"

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

bool IsPathAndroidContent(const std::string& uri)
{
  return StringBeginsWith(uri, "content://");
}

std::string OpenModeToAndroid(std::string mode)
{
  // The 'b' specifier is not supported. Since we're on POSIX, it's fine to just skip it.
  if (!mode.empty() && mode.back() == 'b')
    mode.pop_back();

  if (mode == "r+")
    mode = "rw";
  else if (mode == "w+")
    mode = "rwt";
  else if (mode == "a+")
    mode = "rwa";
  else if (mode == "a")
    mode = "wa";

  return mode;
}

std::string OpenModeToAndroid(std::ios_base::openmode mode)
{
  std::string result;

  if (mode & std::ios_base::in)
    result += 'r';

  if (mode & (std::ios_base::out | std::ios_base::app))
    result += 'w';

  if (mode & std::ios_base::app)
    result += 'a';

  constexpr std::ios_base::openmode t = std::ios_base::in | std::ios_base::trunc;
  if ((mode & t) == t)
    result += 't';

  // The 'b' specifier is not supported. Since we're on POSIX, it's fine to just skip it.

  return result;
}

int OpenAndroidContent(const std::string& uri, const std::string& mode)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticIntMethod(IDCache::GetContentHandlerClass(),
                                  IDCache::GetContentHandlerOpenFd(), ToJString(env, uri),
                                  ToJString(env, mode));
}

bool DeleteAndroidContent(const std::string& uri)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticBooleanMethod(IDCache::GetContentHandlerClass(),
                                      IDCache::GetContentHandlerDelete(), ToJString(env, uri));
}

jlong GetAndroidContentSizeAndIsDirectory(const std::string& uri)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticLongMethod(IDCache::GetContentHandlerClass(),
                                   IDCache::GetContentHandlerGetSizeAndIsDirectory(),
                                   ToJString(env, uri));
}

std::string GetAndroidContentDisplayName(const std::string& uri)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject display_name =
      env->CallStaticObjectMethod(IDCache::GetContentHandlerClass(),
                                  IDCache::GetContentHandlerGetDisplayName(), ToJString(env, uri));
  return display_name ? GetJString(env, reinterpret_cast<jstring>(display_name)) : "";
}

std::vector<std::string> GetAndroidContentChildNames(const std::string& uri)
{
  JNIEnv* env = IDCache::GetEnvForThread();
  jobject children = env->CallStaticObjectMethod(IDCache::GetContentHandlerClass(),
                                                 IDCache::GetContentHandlerGetChildNames(),
                                                 ToJString(env, uri), false);
  return JStringArrayToVector(env, reinterpret_cast<jobjectArray>(children));
}

int GetNetworkIpAddress()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticIntMethod(IDCache::GetNetworkHelperClass(),
                                  IDCache::GetNetworkHelperGetNetworkIpAddress());
}

int GetNetworkPrefixLength()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticIntMethod(IDCache::GetNetworkHelperClass(),
                                  IDCache::GetNetworkHelperGetNetworkPrefixLength());
}

int GetNetworkGateway()
{
  JNIEnv* env = IDCache::GetEnvForThread();
  return env->CallStaticIntMethod(IDCache::GetNetworkHelperClass(),
                                  IDCache::GetNetworkHelperGetNetworkGateway());
}
