// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ios>
#include <string>
#include <string_view>
#include <vector>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr);
jstring ToJString(JNIEnv* env, std::string_view str);

std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray array);
jobjectArray VectorToJStringArray(JNIEnv* env, const std::vector<std::string>& vector);

template <typename T, typename F>
jobjectArray VectorToJObjectArray(JNIEnv* env, const std::vector<T>& vector, jclass clazz, F f)
{
  const auto vector_size = static_cast<jsize>(vector.size());
  jobjectArray result = env->NewObjectArray(vector_size, clazz, nullptr);
  for (jsize i = 0; i < vector_size; ++i)
  {
    jobject obj = f(env, vector[i]);
    env->SetObjectArrayElement(result, i, obj);
    env->DeleteLocalRef(obj);
  }
  return result;
}

// Returns true if the given path should be opened as Android content instead of a normal file.
bool IsPathAndroidContent(std::string_view uri);

// Turns a C/C++ style mode (e.g. "rb") into one which can be used with OpenAndroidContent.
std::string OpenModeToAndroid(std::string mode);
std::string OpenModeToAndroid(std::ios_base::openmode mode);

// Opens a given file and returns a file descriptor.
int OpenAndroidContent(std::string_view uri, std::string_view mode);

// Deletes a given file.
bool DeleteAndroidContent(std::string_view uri);
// Returns -1 if not found, -2 if directory, file size otherwise.
jlong GetAndroidContentSizeAndIsDirectory(std::string_view uri);

// An unmangled URI (one which the C++ code has not appended anything to) can't be relied on
// to contain a file name at all. If a file name is desired, this function is the most reliable
// way to get it, but the display name is not guaranteed to always actually be like a file name.
// An empty string will be returned for files which do not exist.
std::string GetAndroidContentDisplayName(std::string_view uri);

// Returns the display names of all children of a directory, non-recursively.
std::vector<std::string> GetAndroidContentChildNames(std::string_view uri);

std::vector<std::string> DoFileSearchAndroidContent(std::string_view directory,
                                                    const std::vector<std::string>& extensions,
                                                    bool recursive);

int GetNetworkIpAddress();
int GetNetworkPrefixLength();
int GetNetworkGateway();
