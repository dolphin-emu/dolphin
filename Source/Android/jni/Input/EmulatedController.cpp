// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Input/Control.h"
#include "jni/Input/ControlGroup.h"
#include "jni/Input/ControlReference.h"

ControllerEmu::EmulatedController* EmulatedControllerFromJava(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ControllerEmu::EmulatedController*>(
      env->GetLongField(obj, IDCache::GetEmulatedControllerPointer()));
}

static jobject EmulatedControllerToJava(JNIEnv* env, ControllerEmu::EmulatedController* controller)
{
  if (!controller)
    return nullptr;

  return env->NewObject(IDCache::GetEmulatedControllerClass(),
                        IDCache::GetEmulatedControllerConstructor(),
                        reinterpret_cast<jlong>(controller));
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getDefaultDevice(
    JNIEnv* env, jobject obj)
{
  return ToJString(env, EmulatedControllerFromJava(env, obj)->GetDefaultDevice().ToString());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_setDefaultDevice(
    JNIEnv* env, jobject obj, jstring j_device)
{
  return EmulatedControllerFromJava(env, obj)->SetDefaultDevice(GetJString(env, j_device));
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getGroupCount(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(EmulatedControllerFromJava(env, obj)->groups.size());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getGroup(
    JNIEnv* env, jobject obj, jint controller_index)
{
  return ControlGroupToJava(env,
                            EmulatedControllerFromJava(env, obj)->groups[controller_index].get());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_updateSingleControlReference(
    JNIEnv* env, jobject obj, jobject control_reference)
{
  return EmulatedControllerFromJava(env, obj)->UpdateSingleControlReference(
      g_controller_interface, ControlReferenceFromJava(env, control_reference));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getGcPad(
    JNIEnv* env, jclass, jint controller_index)
{
  return EmulatedControllerToJava(env, Pad::GetConfig()->GetController(controller_index));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getWiimote(
    JNIEnv* env, jclass, jint controller_index)
{
  return EmulatedControllerToJava(env, Wiimote::GetConfig()->GetController(controller_index));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getWiimoteAttachment(
    JNIEnv* env, jclass, jint controller_index, jint attachment_index)
{
  auto* attachments = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(controller_index, WiimoteEmu::WiimoteGroup::Attachments));
  return EmulatedControllerToJava(env, attachments->GetAttachmentList()[attachment_index].get());
}
}
