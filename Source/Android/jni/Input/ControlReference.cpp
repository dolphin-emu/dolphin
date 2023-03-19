// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Input/ControlReference.h"

#include <string>

#include <jni.h>

#include "InputCommon/ControlReference/ControlReference.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

ControlReference* ControlReferenceFromJava(JNIEnv* env, jobject control_reference)
{
  return reinterpret_cast<ControlReference*>(
      env->GetLongField(control_reference, IDCache::GetControlReferencePointer()));
}

jobject ControlReferenceToJava(JNIEnv* env, ControlReference* control_reference)
{
  if (!control_reference)
    return nullptr;

  return env->NewObject(IDCache::GetControlReferenceClass(),
                        IDCache::GetControlReferenceConstructor(),
                        reinterpret_cast<jlong>(control_reference));
}

extern "C" {

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlReference_getState(
    JNIEnv* env, jobject obj)
{
  return ControlReferenceFromJava(env, obj)->GetState<double>();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlReference_getExpression(
    JNIEnv* env, jobject obj)
{
  return ToJString(env, ControlReferenceFromJava(env, obj)->GetExpression());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlReference_setExpression(
    JNIEnv* env, jobject obj, jstring expr)
{
  const std::optional<std::string> result =
      ControlReferenceFromJava(env, obj)->SetExpression(GetJString(env, expr));
  return result ? ToJString(env, *result) : nullptr;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlReference_isInput(
    JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(ControlReferenceFromJava(env, obj)->IsInput());
}
}
