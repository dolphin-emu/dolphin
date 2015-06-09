// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <IOKit/hid/IOHIDLib.h>

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

namespace ciface
{
namespace OSX
{

class Joystick : public ForceFeedback::ForceFeedbackDevice
{
private:
	class Button : public Input
	{
	public:
		std::string GetName() const;
		Button(IOHIDElementRef element, IOHIDDeviceRef device)
			: m_element(element), m_device(device) {}
		ControlState GetState() const;
	private:
		const IOHIDElementRef m_element;
		const IOHIDDeviceRef  m_device;
	};

	class Axis : public Input
	{
	public:
		enum direction
		{
			positive = 0,
			negative
		};
		std::string GetName() const;
		Axis(IOHIDElementRef element, IOHIDDeviceRef device, direction dir);
		ControlState GetState() const;

	private:
		const IOHIDElementRef m_element;
		const IOHIDDeviceRef  m_device;
		std::string           m_name;
		const direction       m_direction;
		float                 m_neutral;
		float                 m_scale;
	};

	class Hat : public Input
	{
	public:
		enum direction
		{
			up = 0,
			right,
			down,
			left
		};
		std::string GetName() const;
		Hat(IOHIDElementRef element, IOHIDDeviceRef device, direction dir);
		ControlState GetState() const;

	private:
		const IOHIDElementRef m_element;
		const IOHIDDeviceRef  m_device;
		const char*           m_name;
		const direction       m_direction;
	};

public:
	Joystick(IOHIDDeviceRef device, std::string name, int index);
	~Joystick();

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

private:
	const IOHIDDeviceRef m_device;
	const std::string    m_device_name;
	const int            m_index;

	ForceFeedback::FFDeviceAdapterReference m_ff_device;
};

}
}
