// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

class ControlReference;

jobject ControlReferenceToJava(JNIEnv* env, ControlReference* control_reference);
ControlReference* ControlReferenceFromJava(JNIEnv* env, jobject control_reference);
