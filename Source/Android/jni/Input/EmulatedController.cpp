// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
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
#include "jni/Input/NumericSetting.h"

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

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_loadDefaultSettings(
    JNIEnv* env, jobject obj)
{
  ControllerEmu::EmulatedController* controller = EmulatedControllerFromJava(env, obj);

  controller->LoadDefaults(g_controller_interface);
  controller->UpdateReferences(g_controller_interface);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_clearSettings(
    JNIEnv* env, jobject obj)
{
  ControllerEmu::EmulatedController* controller = EmulatedControllerFromJava(env, obj);

  // Loading an empty IniFile section clears everything.
  Common::IniFile::Section section;

  controller->LoadConfig(&section);
  controller->UpdateReferences(g_controller_interface);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_loadProfile(
    JNIEnv* env, jobject obj, jstring j_path)
{
  ControllerEmu::EmulatedController* controller = EmulatedControllerFromJava(env, obj);

  Common::IniFile ini;
  ini.Load(GetJString(env, j_path));

  controller->LoadConfig(ini.GetOrCreateSection("Profile"));
  controller->UpdateReferences(g_controller_interface);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_saveProfile(
    JNIEnv* env, jobject obj, jstring j_path)
{
  const std::string path = GetJString(env, j_path);

  File::CreateFullPath(path);

  Common::IniFile ini;

  EmulatedControllerFromJava(env, obj)->SaveConfig(ini.GetOrCreateSection("Profile"));
  ini.Save(path);
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

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getSelectedWiimoteAttachment(
    JNIEnv* env, jclass, jint controller_index)
{
  auto* attachments = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(controller_index, WiimoteEmu::WiimoteGroup::Attachments));
  return static_cast<jint>(attachments->GetSelectedAttachment());
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_controlleremu_EmulatedController_getSidewaysWiimoteSetting(
    JNIEnv* env, jclass, jint controller_index)
{
  ControllerEmu::ControlGroup* options =
      Wiimote::GetWiimoteGroup(controller_index, WiimoteEmu::WiimoteGroup::Options);

  for (auto& setting : options->numeric_settings)
  {
    if (setting->GetININame() == WiimoteEmu::Wiimote::SIDEWAYS_OPTION)
      return NumericSettingToJava(env, setting.get());
  }

  return nullptr;
}
}
