// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <IOKit/hid/IOHIDLib.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace OSX
{

class Keyboard : public Core::Device
{
private:
	class Key : public Input
	{
	public:
		std::string GetName() const;
		Key(IOHIDElementRef element, IOHIDDeviceRef device);
		ControlState GetState() const;
	private:
		const IOHIDElementRef m_element;
		const IOHIDDeviceRef  m_device;
		std::string           m_name;
	};

	class Cursor : public Input
	{
	public:
		std::string GetName() const;
		bool IsDetectable() { return false; }
		Cursor(u8 index, const float& axis, const bool positive) : m_axis(axis), m_index(index), m_positive(positive) {}
		ControlState GetState() const;
	private:
		const float& m_axis;
		const u8     m_index;
		const bool   m_positive;
	};

	class Button : public Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const unsigned char& button) : m_button(button), m_index(index) {}
		ControlState GetState() const;
	private:
		const unsigned char& m_button;
		const u8 m_index;
	};

public:
	void UpdateInput() override;

	Keyboard(IOHIDDeviceRef device, std::string name, int index, void *window);

	std::string GetName() const;
	std::string GetSource() const;
	int GetId() const;

private:
	struct
	{
		float x, y;
	} m_cursor;

	const IOHIDDeviceRef m_device;
	const std::string    m_device_name;
	int                  m_index;
	uint32_t             m_windowid;
	unsigned char        m_mousebuttons[3];
};

}
}
