// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/VRTracker.h"

static const char* const named_positions[] =
{
	"Pos X",
	"Pos Y",
	"Pos Z"
};

static const char* const named_orientations[] =
{
	"Quat W",
	"Quat X",
	"Quat Y",
	"Quat Z"
};

namespace VRTracker
{
	static ciface::Core::Device* s_device = nullptr;
	static ControllerInterface::ControlReference* s_position_inputs[3];
	static ControllerInterface::ControlReference* s_orientation_inputs[4];

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
		g_controller_interface.Initialize(hwnd);

		// find the OSVR head tracker inputs
		ciface::Core::DeviceQualifier devq("OSVR", 1, "me/head");

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
	}

	void GetTransformMatrix(Matrix44& mtx)
	{
		Matrix44::LoadIdentity(mtx);

		if (!s_device)
			return;

		s_device->UpdateInput();

		float position[3], orientation[4];

		// Get the inverted position states
		for (int i = 0; i < 3; i++)
			position[i] = (float)-s_position_inputs[i]->State();

		// Get the orientation quaternion
		for (int i = 0; i < 4; i++)
			orientation[i] = (float)s_orientation_inputs[i]->State();

		Quaternion rotationQuat;
		Matrix33 rotationMtx;

		Quaternion::Set(rotationQuat, orientation);
		Quaternion::Invert(rotationQuat);
		Matrix33::LoadQuaternion(rotationMtx, rotationQuat);

		Matrix44 translateMtx, transformMtx;
		Matrix44::LoadMatrix33(transformMtx, rotationMtx);
		Matrix44::Translate(translateMtx, position);
		Matrix44::Multiply(transformMtx, translateMtx, mtx);
	}

}
