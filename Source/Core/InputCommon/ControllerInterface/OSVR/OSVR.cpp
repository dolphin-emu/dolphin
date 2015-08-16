// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/OSVR/OSVR.h"

namespace ciface
{
namespace OSVR
{

static OSVR_ClientContext s_context;

static const char* const tracker_paths[] =
{
	"/me/head",
	"/me/hands/left",
	"/me/hands/right"
};

static const char* const button_paths[OSVR_NUM_BUTTONS] =
{
	"/controller/left/1",
	"/controller/left/2",
	"/controller/left/3",
	"/controller/left/4",
	"/controller/left/bumper",
	"/controller/left/joystick/button",
	"/controller/left/middle",
	"/controller/right/1",
	"/controller/right/2",
	"/controller/right/3",
	"/controller/right/4",
	"/controller/right/bumper",
	"/controller/right/joystick/button",
	"/controller/right/middle"
};

static const char* const axis_paths[OSVR_NUM_AXES] =
{
	"/controller/left/joystick/x",
	"/controller/left/joystick/y",
	"/controller/right/joystick/x",
	"/controller/right/joystick/y"
};

static const char* const trigger_paths[OSVR_NUM_TRIGGERS] =
{
	"/controller/left/trigger",
	"/controller/right/trigger"
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

static const char* const named_buttons[OSVR_NUM_BUTTONS] =
{
	"Left 1",
	"Left 2",
	"Left 3",
	"Left 4",
	"Left Bumper",
	"Left Joystick",
	"Left Middle",
	"Right 1",
	"Right 2",
	"Right 3",
	"Right 4",
	"Right Bumper",
	"Right Joystick",
	"Right Middle"
};

static const char* const named_axes[OSVR_NUM_AXES] =
{
	"Left X",
	"Left Y",
	"Right X",
	"Right Y",
};

static const char* const named_triggers[OSVR_NUM_TRIGGERS] =
{
	"Left Trigger",
	"Right Trigger"
};

void Init(std::vector<Core::Device*>& devices)
{
	s_context = osvrClientInit("org.dolphin-emu.InputCommon");

	int i = 0;
	for (; i < sizeof(tracker_paths) / sizeof(*tracker_paths); ++i)
	{
		OSVR_ClientInterface device;
		if (osvrClientGetInterface(s_context, tracker_paths[i], &device) == OSVR_RETURN_SUCCESS)
			devices.push_back(new Tracker(device, tracker_paths[i], i));
	}

	OSVR_ClientInterface device;
	if (osvrClientGetInterface(s_context, "/controller", &device) == OSVR_RETURN_SUCCESS)
		devices.push_back(new Controller(device, "/controller", i));
}

void DeInit()
{
	osvrClientShutdown(s_context);
}

void Tracker::TrackerCallback(void* userdata, const OSVR_TimeValue* timestamp, const OSVR_PoseReport* report)
{
	Tracker* device = (Tracker*)userdata;
	device->m_last_update = *timestamp;
	device->m_pose_report = *report;
}

Tracker::Tracker(const OSVR_ClientInterface& device, const char* path, u8 index)
	: m_device(device), m_path(path), m_index(index)
{
	memset(&m_last_update, 0, sizeof(m_last_update));
	memset(&m_pose_report, 0, sizeof(m_pose_report));
	osvrQuatSetIdentity(&m_pose_report.pose.rotation);
	osvrRegisterPoseCallback(device, TrackerCallback, this);

	// get supported buttons
	for (int i = 0; i != sizeof(named_positions) / sizeof(*named_positions); ++i)
	{
		AddInput(new Position(i, m_pose_report.pose.translation.data[i]));
	}

	// get supported axes
	for (int i = 0; i != sizeof(named_orientations) / sizeof(*named_orientations); ++i)
	{
		AddInput(new Orientation(i, m_pose_report.pose.rotation.data[i]));
	}
}

std::string Tracker::GetName() const
{
	// skip the root slash
	return m_path + 1;
}

int Tracker::GetId() const
{
	return m_index;
}

std::string Tracker::GetSource() const
{
	return "OSVR";
}

void Controller::ButtonCallback(void *userdata, const struct OSVR_TimeValue *timestamp, const struct OSVR_ButtonReport *report)
{
	*((OSVR_ButtonState*)userdata) = report->state;
}

void Controller::AnalogCallback(void *userdata, const struct OSVR_TimeValue *timestamp, const struct OSVR_AnalogReport *report)
{
	*((OSVR_AnalogState*)userdata) = report->state;
}

Controller::Controller(const OSVR_ClientInterface& device, const char* path, u8 index)
	: m_device(device), m_path(path), m_index(index)
{
	memset(&m_buttons, 0, sizeof(m_buttons));
	memset(&m_axes, 0, sizeof(m_axes));
	memset(&m_triggers, 0, sizeof(m_triggers));

	// get supported buttons
	for (int i = 0; i < OSVR_NUM_BUTTONS; i++)
	{
		OSVR_ClientInterface button;
		if (osvrClientGetInterface(s_context, button_paths[i], &button) == OSVR_RETURN_SUCCESS &&
			osvrRegisterButtonCallback(button, &ButtonCallback, &m_buttons[i]) == OSVR_RETURN_SUCCESS)
			AddInput(new Button(i, m_buttons[i]));
	}

	// get supported axes
	for (int i = 0; i < OSVR_NUM_AXES; i++)
	{
		OSVR_ClientInterface axis;
		if (osvrClientGetInterface(s_context, axis_paths[i], &axis) == OSVR_RETURN_SUCCESS &&
			osvrRegisterAnalogCallback(axis, &AnalogCallback, &m_axes[i]) == OSVR_RETURN_SUCCESS)
		{
			AddInput(new Axis(i, m_axes[i], true));
			AddInput(new Axis(i, m_axes[i], false));
		}
	}

	// get supported triggers
	for (int i = 0; i < OSVR_NUM_TRIGGERS; i++)
	{
		OSVR_ClientInterface trigger;
		if (osvrClientGetInterface(s_context, trigger_paths[i], &trigger) == OSVR_RETURN_SUCCESS &&
			osvrRegisterAnalogCallback(trigger, &AnalogCallback, &m_triggers[i]) == OSVR_RETURN_SUCCESS)
			AddInput(new Trigger(i, m_triggers[i]));
	}
}

std::string Controller::GetName() const
{
	// skip the root slash
	return m_path + 1;
}

int Controller::GetId() const
{
	return m_index;
}

std::string Controller::GetSource() const
{
	return "OSVR";
}

// Update I/O

void Tracker::UpdateInput()
{
	osvrClientUpdate(s_context);
}

void Controller::UpdateInput()
{
	osvrClientUpdate(s_context);
}

// GET name/source/id

std::string Tracker::Position::GetName() const
{
	return named_positions[m_index];
}

std::string Tracker::Orientation::GetName() const
{
	return named_orientations[m_index];
}

std::string Controller::Button::GetName() const
{
	return named_buttons[m_index];
}

std::string Controller::Axis::GetName() const
{
	return std::string(named_axes[m_index]) + (m_negative ? '-' : '+');
}

std::string Controller::Trigger::GetName() const
{
	return named_triggers[m_index];
}

// GET / SET STATES

ControlState Tracker::Position::GetState() const
{
	return m_axis;
}

ControlState Tracker::Orientation::GetState() const
{
	return m_axis;
}

ControlState Controller::Button::GetState() const
{
	return m_button == OSVR_BUTTON_PRESSED;
}

ControlState Controller::Axis::GetState() const
{
	return std::max(0.0, ControlState(m_axis) * (m_negative ? -1.0 : 1.0));
}

ControlState Controller::Trigger::GetState() const
{
	return m_trigger;
}

}
}
