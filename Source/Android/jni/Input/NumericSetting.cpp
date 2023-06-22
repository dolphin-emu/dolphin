// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Input/NumericSetting.h"

#include <string>

#include <jni.h>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Input/ControlReference.h"

static const char* NullStringToEmptyString(const char* str)
{
  return str ? str : "";
}

static ControllerEmu::NumericSettingBase* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ControllerEmu::NumericSettingBase*>(
      env->GetLongField(obj, IDCache::GetNumericSettingPointer()));
}

template <typename T>
static ControllerEmu::NumericSetting<T>* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ControllerEmu::NumericSetting<T>*>(
      env->GetLongField(obj, IDCache::GetNumericSettingPointer()));
}

jobject NumericSettingToJava(JNIEnv* env, ControllerEmu::NumericSettingBase* numeric_setting)
{
  if (!numeric_setting)
    return nullptr;

  return env->NewObject(IDCache::GetNumericSettingClass(), IDCache::GetNumericSettingConstructor(),
                        reinterpret_cast<jlong>(numeric_setting));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getUiName(
    JNIEnv* env, jobject obj)
{
  return ToJString(env, Common::GetStringT(GetPointer(env, obj)->GetUIName()));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getUiSuffix(
    JNIEnv* env, jobject obj)
{
  const char* str = NullStringToEmptyString(GetPointer(env, obj)->GetUISuffix());
  return ToJString(env, Common::GetStringT(str));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getUiDescription(
    JNIEnv* env, jobject obj)
{
  const char* str = NullStringToEmptyString(GetPointer(env, obj)->GetUIDescription());
  return ToJString(env, Common::GetStringT(str));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getType(
    JNIEnv* env, jobject obj)
{
  return static_cast<int>(GetPointer(env, obj)->GetType());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getControlReference(
    JNIEnv* env, jobject obj)
{
  return ControlReferenceToJava(env, &GetPointer(env, obj)->GetInputReference());
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getIntValue(
    JNIEnv* env, jobject obj)
{
  return GetPointer<int>(env, obj)->GetValue();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_setIntValue(
    JNIEnv* env, jobject obj, jint value)
{
  GetPointer<int>(env, obj)->SetValue(value);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getIntDefaultValue(
    JNIEnv* env, jobject obj)
{
  return GetPointer<int>(env, obj)->GetDefaultValue();
}

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getDoubleValue(
    JNIEnv* env, jobject obj)
{
  return GetPointer<double>(env, obj)->GetValue();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_setDoubleValue(
    JNIEnv* env, jobject obj, jdouble value)
{
  GetPointer<double>(env, obj)->SetValue(value);
}

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getDoubleDefaultValue(
    JNIEnv* env, jobject obj)
{
  return GetPointer<double>(env, obj)->GetDefaultValue();
}

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getDoubleMin(
    JNIEnv* env, jobject obj)
{
  return GetPointer<double>(env, obj)->GetMinValue();
}

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getDoubleMax(
    JNIEnv* env, jobject obj)
{
  return GetPointer<double>(env, obj)->GetMaxValue();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getBooleanValue(
    JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer<bool>(env, obj)->GetValue());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_setBooleanValue(
    JNIEnv* env, jobject obj, jboolean value)
{
  GetPointer<bool>(env, obj)->SetValue(static_cast<bool>(value));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_NumericSetting_getBooleanDefaultValue(
    JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer<bool>(env, obj)->GetDefaultValue());
}
}
