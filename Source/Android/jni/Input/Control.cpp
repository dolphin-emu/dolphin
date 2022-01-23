// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/Input/Control.h"

#include <string>

#include <jni.h>

#include "Common/MsgHandler.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Input/ControlReference.h"

static ControllerEmu::Control* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ControllerEmu::Control*>(
      env->GetLongField(obj, IDCache::GetControlPointer()));
}

jobject ControlToJava(JNIEnv* env, ControllerEmu::Control* control)
{
  if (!control)
    return nullptr;

  return env->NewObject(IDCache::GetControlClass(), IDCache::GetControlConstructor(),
                        reinterpret_cast<jlong>(control));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_Control_getUiName(JNIEnv* env,
                                                                                    jobject obj)
{
  ControllerEmu::Control* control = GetPointer(env, obj);
  std::string ui_name = control->ui_name;
  if (control->translate == ControllerEmu::Translatability::Translate)
    ui_name = Common::GetStringT(ui_name.c_str());
  return ToJString(env, ui_name);
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_Control_getControlReference(
    JNIEnv* env, jobject obj)
{
  return ControlReferenceToJava(env, GetPointer(env, obj)->control_ref.get());
}
}
