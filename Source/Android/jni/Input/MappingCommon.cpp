// Copyright 2022 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <string>
#include <vector>

#include <jni.h>

#include "Core/FreeLookManager.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/Input/EmulatedController.h"

namespace
{
constexpr auto INPUT_DETECT_INITIAL_TIME = std::chrono::seconds(3);
constexpr auto INPUT_DETECT_CONFIRMATION_TIME = std::chrono::milliseconds(0);
constexpr auto INPUT_DETECT_MAXIMUM_TIME = std::chrono::seconds(5);
}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_MappingCommon_detectInput(
    JNIEnv* env, jclass, jobject j_emulated_controller, jboolean all_devices)
{
  ControllerEmu::EmulatedController* emulated_controller =
      EmulatedControllerFromJava(env, j_emulated_controller);

  const ciface::Core::DeviceQualifier default_device = emulated_controller->GetDefaultDevice();

  std::vector<std::string> device_strings;
  if (all_devices)
    device_strings = g_controller_interface.GetAllDeviceStrings();
  else
    device_strings = {default_device.ToString()};

  auto detections =
      g_controller_interface.DetectInput(device_strings, INPUT_DETECT_INITIAL_TIME,
                                         INPUT_DETECT_CONFIRMATION_TIME, INPUT_DETECT_MAXIMUM_TIME);

  ciface::MappingCommon::RemoveSpuriousTriggerCombinations(&detections);

  return ToJString(env, ciface::MappingCommon::BuildExpression(detections, default_device,
                                                               ciface::MappingCommon::Quote::On));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_MappingCommon_getExpressionForControl(
    JNIEnv* env, jclass, jstring j_control, jstring j_device, jstring j_default_device)
{
  ciface::Core::DeviceQualifier device_qualifier, default_device_qualifier;
  device_qualifier.FromString(GetJString(env, j_device));
  default_device_qualifier.FromString(GetJString(env, j_default_device));

  return ToJString(env, ciface::MappingCommon::GetExpressionForControl(GetJString(env, j_control),
                                                                       device_qualifier,
                                                                       default_device_qualifier));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_MappingCommon_save(JNIEnv* env, jclass)
{
  Pad::GetConfig()->SaveConfig();
  Pad::GetGBAConfig()->SaveConfig();
  Keyboard::GetConfig()->SaveConfig();
  Wiimote::GetConfig()->SaveConfig();
  HotkeyManagerEmu::GetConfig()->SaveConfig();
  FreeLook::GetInputConfig()->SaveConfig();
}
};
