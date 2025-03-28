// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

namespace
{
constexpr auto INPUT_DETECT_INITIAL_TIME = std::chrono::seconds(3);
constexpr auto INPUT_DETECT_CONFIRMATION_TIME = std::chrono::milliseconds(0);
constexpr auto INPUT_DETECT_MAXIMUM_TIME = std::chrono::seconds(5);
}  // namespace

static ciface::Core::InputDetector* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ciface::Core::InputDetector*>(
      env->GetLongField(obj, IDCache::GetInputDetectorPointer()));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_finalize(JNIEnv* env, jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_createNew(JNIEnv*, jobject)
{
  return reinterpret_cast<jlong>(new ciface::Core::InputDetector);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_start(
    JNIEnv* env, jobject obj, jstring j_default_device, jboolean all_devices)
{
  std::vector<std::string> device_strings;
  if (all_devices)
    device_strings = g_controller_interface.GetAllDeviceStrings();
  else
    device_strings = {GetJString(env, j_default_device)};

  GetPointer(env, obj)->Start(g_controller_interface, device_strings);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_update(JNIEnv* env, jobject obj)
{
  GetPointer(env, obj)->Update(INPUT_DETECT_INITIAL_TIME, INPUT_DETECT_CONFIRMATION_TIME,
                               INPUT_DETECT_MAXIMUM_TIME);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_isComplete(JNIEnv* env,
                                                                             jobject obj)
{
  return GetPointer(env, obj)->IsComplete();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputDetector_takeResults(
    JNIEnv* env, jobject obj, jstring j_default_device)
{
  ciface::Core::DeviceQualifier default_device;
  default_device.FromString(GetJString(env, j_default_device));

  auto detections = GetPointer(env, obj)->TakeResults();

  ciface::MappingCommon::RemoveSpuriousTriggerCombinations(&detections);

  return ToJString(env, ciface::MappingCommon::BuildExpression(detections, default_device,
                                                               ciface::MappingCommon::Quote::On));
}
}
