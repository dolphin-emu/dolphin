// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoCommon/VRTracker.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControlReference/ControlReference.h"


static const char* const named_positions[] = {"Pos X", "Pos Y", "Pos Z"};

static const char* const named_orientations[] = {"Quat W", "Quat X", "Quat Y", "Quat Z"};

namespace VRTracker
{
static ciface::Core::Device* s_device = nullptr;
static ControlReference* s_position_inputs[3];
static ControlReference* s_orientation_inputs[4];

static float s_initial_position[3];
static float s_initial_orientation[4];

void Shutdown()
{
  for (int i = 0; i < 3; i++)
    delete s_position_inputs[i];

  for (int i = 0; i < 4; i++)
    delete s_orientation_inputs[i];

  s_device = nullptr;
  g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and find a tracker
void Initialize(void* const hwnd)
{
#if 0
  g_controller_interface.Initialize(hwnd);

  // find the OSVR head tracker inputs
  ciface::Core::DeviceQualifier devq("OSVR", 0, "me/head");

  s_device = g_controller_interface.FindDevice(devq);

  for (int i = 0; i < 3; i++)
  {
    s_position_inputs[i] = new ControllerInterface::InputReference();
    s_position_inputs[i]->expression = named_positions[i];
    g_controller_interface.UpdateReference(s_position_inputs[i], devq);
  }

  for (int i = 0; i < 4; i++)
  {
    s_orientation_inputs[i] = new ControllerInterface::InputReference();
    s_orientation_inputs[i]->expression = named_orientations[i];
    g_controller_interface.UpdateReference(s_orientation_inputs[i], devq);
  }
#endif
}

void ResetView()
{
  if (!s_device)
    return;

  s_device->UpdateInput();

  for (int i = 0; i < 3; i++)
    s_initial_position[i] = (float)s_position_inputs[i]->State();

  for (int i = 0; i < 4; i++)
    s_initial_orientation[i] = (float)s_orientation_inputs[i]->State();
}

void GetTransformMatrix(Matrix44& mtx)
{
  Matrix44::LoadIdentity(mtx);

  if (!s_device)
    return;

  // Make sure we have the most recent state available
  s_device->UpdateInput();

  float position[3], orientation[4];

  // Get the inverted position states offset from the initial position
  for (int i = 0; i < 3; i++)
    position[i] = -((float)s_position_inputs[i]->State() - s_initial_position[i]);

  // Get the orientation quaternion
  for (int i = 0; i < 4; i++)
    orientation[i] = (float)s_orientation_inputs[i]->State();

  Quaternion worldQuat, initialQuat, orientationQuat;
  Matrix33 worldMtx;

  // Get the initial and current orientation quaternions
  Quaternion::Set(orientationQuat, orientation);
  Quaternion::Set(initialQuat, s_initial_orientation);

  // Rotate the current orientation by the inverse of the initial orientation
  Quaternion::Invert(initialQuat);
  initialQuat.data[1] = 0.0f;
  initialQuat.data[3] = 0.0f;
  Quaternion::Multiply(initialQuat, orientationQuat, worldQuat);

  // Invert the quaternion to get the world orientation
  Quaternion::Invert(worldQuat);
  Matrix33::LoadQuaternion(worldMtx, worldQuat);

  // Translate the transformation matrix by the position
  Matrix44 translateMtx, transformationMtx;
  Matrix44::LoadMatrix33(transformationMtx, worldMtx);
  Matrix44::Translate(translateMtx, position);
  Matrix44::Multiply(transformationMtx, translateMtx, mtx);
}
}
