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
		Key(IOHIDElementRef element, IOHIDDeviceRef device);
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const IOHIDElementRef m_element;
		const IOHIDDeviceRef  m_device;
		std::string           m_name;
	};

	class Cursor : public Input
	{
	public:
		Cursor(u8 index, const float& axis, const bool positive) : m_axis(axis), m_index(index), m_positive(positive) {}
		std::string GetName() const override;
		bool IsDetectable() override { return false; }
		ControlState GetState() const override;
	private:
		const float& m_axis;
		const u8     m_index;
		const bool   m_positive;
	};

	class Button : public Input
	{
	public:
		Button(u8 index, const unsigned char& button) : m_button(button), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const unsigned char& m_button;
		const u8 m_index;
	};

public:
	void UpdateInput() override;

	Keyboard(IOHIDDeviceRef device, std::string name, int index, void *window);

	std::string GetName() const override;
	std::string GetSource() const override;
	int GetId() const override;

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
