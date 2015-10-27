// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/DInput/DInput8.h"

namespace ciface
{
namespace DInput
{

void InitKeyboardMouse(IDirectInput8* const idi8, std::vector<Core::Device*>& devices, HWND _hwnd);

class KeyboardMouse : public Core::Device
{
private:
	struct State
	{
		BYTE          keyboard[256];
		DIMOUSESTATE2 mouse;
		struct
		{
			ControlState x, y;
		} cursor;
	};

	class Key : public Input
	{
	public:
		Key(u8 index, const BYTE& key) : m_key(key), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const BYTE& m_key;
		const u8 m_index;
	};

	class Button : public Input
	{
	public:
		Button(u8 index, const BYTE& button) : m_button(button), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const BYTE& m_button;
		const u8 m_index;
	};

	class Axis : public Input
	{
	public:
		Axis(u8 index, const LONG& axis, LONG range) : m_axis(axis), m_range(range), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const LONG& m_axis;
		const LONG m_range;
		const u8 m_index;
	};

	class Cursor : public Input
	{
	public:
		Cursor(u8 index, const ControlState& axis, const bool positive) :  m_axis(axis), m_index(index), m_positive(positive) {}
		std::string GetName() const override;
		bool IsDetectable() override { return false; }
		ControlState GetState() const override;
	private:
		const ControlState& m_axis;
		const u8 m_index;
		const bool m_positive;
	};

public:
	void UpdateInput() override;

	KeyboardMouse(const LPDIRECTINPUTDEVICE8 kb_device, const LPDIRECTINPUTDEVICE8 mo_device);
	~KeyboardMouse();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	const LPDIRECTINPUTDEVICE8 m_kb_device;
	const LPDIRECTINPUTDEVICE8 m_mo_device;

	DWORD         m_last_update;
	State         m_state_in;
};

}
}
