// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <ios>
#include <string>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr);
jstring ToJString(JNIEnv* env, const std::string& str);
std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray array);

// Returns true if the given path should be opened as Android content instead of a normal file.
bool IsPathAndroidContent(const std::string& uri);

// Turns a C/C++ style mode (e.g. "rb") into one which can be used with OpenAndroidContent.
std::string OpenModeToAndroid(std::string mode);
std::string OpenModeToAndroid(std::ios_base::openmode mode);

// Opens a given file and returns a file descriptor.
int OpenAndroidContent(const std::string& uri, const std::string& mode);

// Deletes a given file.
bool DeleteAndroidContent(const std::string& uri);
// Returns -1 if not found, -2 if directory, file size otherwise.
jlong GetAndroidContentSizeAndIsDirectory(const std::string& uri);

int GetNetworkIpAddress();
int GetNetworkPrefixLength();
int GetNetworkGateway();
