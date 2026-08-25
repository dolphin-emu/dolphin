// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <jni.h>

#include "InputCommon/ControllerInterface/Touch/InputOverrider.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_registerGameCube(
    JNIEnv*, jclass, int controller_index)
{
  ciface::Touch::RegisterGameCubeInputOverrider(controller_index);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_registerWii(JNIEnv*, jclass,
                                                                               int controller_index)
{
  ciface::Touch::RegisterWiiInputOverrider(controller_index);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_unregisterGameCube(
    JNIEnv*, jclass, int controller_index)
{
  ciface::Touch::UnregisterGameCubeInputOverrider(controller_index);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_unregisterWii(
    JNIEnv*, jclass, int controller_index)
{
  ciface::Touch::UnregisterWiiInputOverrider(controller_index);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_setControlState(
    JNIEnv*, jclass, int controller_index, int control, double state)
{
  ciface::Touch::SetControlState(controller_index, static_cast<ciface::Touch::ControlID>(control),
                                 state);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_clearControlState(
    JNIEnv*, jclass, int controller_index, int control)
{
  ciface::Touch::ClearControlState(controller_index,
                                   static_cast<ciface::Touch::ControlID>(control));
}

JNIEXPORT double JNICALL
Java_org_dolphinemu_dolphinemu_features_input_model_InputOverrider_getGateRadiusAtAngle(
    JNIEnv*, jclass, int controller_index, int stick, double angle)
{
  const auto casted_stick = static_cast<ciface::Touch::ControlID>(stick);
  return ciface::Touch::GetGateRadiusAtAngle(controller_index, casted_stick, angle);
}
};
