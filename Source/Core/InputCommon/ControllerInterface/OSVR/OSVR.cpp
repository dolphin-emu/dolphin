// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/OSVR/OSVR.h"

namespace ciface
{
namespace OSVR
{

static OSVR_ClientContext s_context;

static const char* const interface_paths[] =
{
	"/me/head",
	"/me/hands/left",
	"/me/hands/right"
};

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

void Init(std::vector<Core::Device*>& devices)
{
	s_context = osvrClientInit("org.dolphin-emu.InputCommon");

	for (int i = 0; i < sizeof(interface_paths) / sizeof(*interface_paths); ++i)
	{
		OSVR_ClientInterface device;
		if (osvrClientGetInterface(s_context, interface_paths[i], &device) == OSVR_RETURN_SUCCESS)
			devices.push_back(new Device(device, interface_paths[i], i));
	}
}

void DeInit()
{
	osvrClientShutdown(s_context);
}

void Device::TrackerCallback(void* userdata, const OSVR_TimeValue* timestamp, const OSVR_PoseReport* report)
{
	Device* device = (Device*)userdata;
	device->m_last_update = *timestamp;
	device->m_pose_report = *report;
}

Device::Device(const OSVR_ClientInterface& device, const char* path, u8 index)
	: m_device(device), m_path(path), m_index(index)
{
	memset(&m_pose_report, 0, sizeof(m_pose_report));

	osvrRegisterPoseCallback(m_device, &TrackerCallback, this);

	// get supported positions
	for (int i = 0; i != sizeof(named_positions) / sizeof(*named_positions); ++i)
	{
		AddInput(new Position(i, m_pose_report.pose.translation.data[i]));
	}

	// get supported orientations
	for (int i = 0; i != sizeof(named_orientations) / sizeof(*named_orientations); ++i)
	{
		AddInput(new Orientation(i, m_pose_report.pose.rotation.data[i]));
	}
}

std::string Device::GetName() const
{
	// skip the root slash
	return m_path + 1;
}

int Device::GetId() const
{
	return m_index;
}

std::string Device::GetSource() const
{
	return "OSVR";
}

// Update I/O

void Device::UpdateInput()
{
	osvrClientUpdate(s_context);
}

// GET name/source/id

std::string Device::Position::GetName() const
{
	return named_positions[m_index];
}

std::string Device::Orientation::GetName() const
{
	return named_orientations[m_index];
}

// GET / SET STATES

ControlState Device::Position::GetState() const
{
	return m_axis;
}

ControlState Device::Orientation::GetState() const
{
	return m_axis;
}

}
}
