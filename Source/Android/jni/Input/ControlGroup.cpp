// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Input/ControlGroup.h"

#include <jni.h>

#include "Common/MsgHandler.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Input/Control.h"
#include "jni/Input/NumericSetting.h"

static ControllerEmu::ControlGroup* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ControllerEmu::ControlGroup*>(
      env->GetLongField(obj, IDCache::GetControlGroupPointer()));
}

jobject ControlGroupToJava(JNIEnv* env, ControllerEmu::ControlGroup* group)
{
  if (!group)
    return nullptr;

  return env->NewObject(IDCache::GetControlGroupClass(), IDCache::GetControlGroupConstructor(),
                        reinterpret_cast<jlong>(group));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getUiName(
    JNIEnv* env, jobject obj)
{
  return ToJString(env, Common::GetStringT(GetPointer(env, obj)->ui_name.c_str()));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getGroupType(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->type);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getDefaultEnabledValue(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->default_value);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getEnabled(
    JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->enabled);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_setEnabled(
    JNIEnv* env, jobject obj, jboolean value)
{
  GetPointer(env, obj)->enabled = value;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getControlCount(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->controls.size());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getControl(
    JNIEnv* env, jobject obj, jint i)
{
  return ControlToJava(env, GetPointer(env, obj)->controls[i].get());
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getNumericSettingCount(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->numeric_settings.size());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getNumericSetting(
    JNIEnv* env, jobject obj, jint i)
{
  return NumericSettingToJava(env, GetPointer(env, obj)->numeric_settings[i].get());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_ControlGroup_getAttachmentSetting(
    JNIEnv* env, jobject obj)
{
  auto* group = reinterpret_cast<ControllerEmu::Attachments*>(GetPointer(env, obj));
  return NumericSettingToJava(env, &group->GetSelectionSetting());
}
}
