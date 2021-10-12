// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ios>
#include <string>
#include <vector>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr);
jstring ToJString(JNIEnv* env, const std::string& str);

std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray array);
jobjectArray VectorToJStringArray(JNIEnv* env, std::vector<std::string> vector);

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

// An unmangled URI (one which the C++ code has not appended anything to) can't be relied on
// to contain a file name at all. If a file name is desired, this function is the most reliable
// way to get it, but the display name is not guaranteed to always actually be like a file name.
// An empty string will be returned for files which do not exist.
std::string GetAndroidContentDisplayName(const std::string& uri);

// Returns the display names of all children of a directory, non-recursively.
std::vector<std::string> GetAndroidContentChildNames(const std::string& uri);

std::vector<std::string> DoFileSearchAndroidContent(const std::string& directory,
                                                    const std::vector<std::string>& extensions,
                                                    bool recursive);

int GetNetworkIpAddress();
int GetNetworkPrefixLength();
int GetNetworkGateway();
