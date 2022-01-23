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

static ControllerEmu::EmulatedController* GetPointer(JNIEnv* env, jobject obj)
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

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getGroupCount(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->groups.size());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getGroup(
    JNIEnv* env, jobject obj, jint controller_index)
{
  return ControlGroupToJava(env, GetPointer(env, obj)->groups[controller_index].get());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_updateSingleControlReference(
    JNIEnv* env, jobject obj, jobject control_reference)
{
  return GetPointer(env, obj)->UpdateSingleControlReference(
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
