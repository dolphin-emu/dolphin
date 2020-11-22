// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr);
jstring ToJString(JNIEnv* env, const std::string& str);
std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray array);

int OpenAndroidContent(const std::string& uri, const std::string& mode);
bool DeleteAndroidContent(const std::string& uri);
