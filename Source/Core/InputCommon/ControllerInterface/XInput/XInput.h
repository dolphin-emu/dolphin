// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// XInput suffers a similar issue as XAudio2. Since Win8, it is part of the OS.
// However, unlike XAudio2 they have not made the API incompatible - so we just
// compile against the latest version and fall back to dynamically loading the
// old DLL.

#pragma once

#include <windows.h>
#include <XInput.h>

#include "InputCommon/ControllerInterface/Device.h"

#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
#error You are building this module against the wrong version of DirectX. You probably need to remove DXSDK_DIR from your include path and/or _WIN32_WINNT is wrong.
#endif

namespace ciface
{
namespace XInput
{

void Init(std::vector<Core::Device*>& devices);
void DeInit();

class Device : public Core::Device
{
private:
	class Button : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const WORD& buttons) : m_index(index), m_buttons(buttons) {}
		ControlState GetState() const;
		u32 GetStates() const;
	private:
		const WORD& m_buttons;
		u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, const SHORT& axis, SHORT range) : m_index(index), m_axis(axis), m_range(range) {}
		ControlState GetState() const;
	private:
		const SHORT& m_axis;
		const SHORT m_range;
		const u8 m_index;
	};

	class Trigger : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Trigger(u8 index, const BYTE& trigger, BYTE range) : m_index(index), m_trigger(trigger), m_range(range) {}
		ControlState GetState() const;
	private:
		const BYTE& m_trigger;
		const BYTE m_range;
		const u8 m_index;
	};

	class Motor : public Core::Device::Output
	{
	public:
		std::string GetName() const;
		Motor(u8 index, Device* parent, WORD &motor, WORD range) : m_index(index), m_parent(parent), m_motor(motor), m_range(range) {}
		void SetState(ControlState state);
	private:
		WORD& m_motor;
		const WORD m_range;
		const u8 m_index;
		Device* m_parent;
	};

public:
	void UpdateInput() override;

	Device(const XINPUT_CAPABILITIES& capabilities, u8 index);

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

	void UpdateMotors();

private:
	XINPUT_STATE m_state_in;
	XINPUT_VIBRATION m_state_out;
	const BYTE m_subtype;
	const u8 m_index;
};


}
}
