// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

namespace ciface
{
namespace DInput
{

void InitJoystick(IDirectInput8* const idi8, std::vector<Core::Device*>& devices, HWND hwnd);

class Joystick : public ForceFeedback::ForceFeedbackDevice
{
private:
	class Button : public Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const BYTE& button) : m_index(index), m_button(button) {}
		ControlState GetState() const;
	private:
		const BYTE& m_button;
		const u8 m_index;
	};

	class Axis : public Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, const LONG& axis, LONG base, LONG range) : m_index(index), m_axis(axis), m_base(base), m_range(range) {}
		ControlState GetState() const;
	private:
		const LONG& m_axis;
		const LONG m_base, m_range;
		const u8 m_index;
	};

	class Hat : public Input
	{
	public:
		std::string GetName() const;
		Hat(u8 index, const DWORD& hat, u8 direction) : m_index(index), m_hat(hat), m_direction(direction) {}
		ControlState GetState() const;
	private:
		const DWORD& m_hat;
		const u8 m_index, m_direction;
	};

public:
	void UpdateInput() override;

	Joystick(const LPDIRECTINPUTDEVICE8 device, const unsigned int index);
	~Joystick();

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	const LPDIRECTINPUTDEVICE8 m_device;
	const unsigned int         m_index;

	DIJOYSTATE                 m_state_in;

	bool                       m_buffered;
};

}
}
