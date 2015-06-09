// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace Xlib
{

void Init(std::vector<Core::Device*>& devices, void* const hwnd);

class KeyboardMouse : public Core::Device
{

private:
	struct State
	{
		char keyboard[32];
		unsigned int buttons;
		struct
		{
			float x, y;
		} cursor;
	};

	class Key : public Input
	{
		friend class KeyboardMouse;
	public:
		std::string GetName() const override;
		Key(Display* display, KeyCode keycode, const char* keyboard);
		ControlState GetState() const override;

	private:
		std::string       m_keyname;
		Display* const    m_display;
		const char* const m_keyboard;
		const KeyCode     m_keycode;
	};

	class Button : public Input
	{
	public:
		std::string GetName() const override;
		Button(unsigned int index, unsigned int& buttons)
			: m_buttons(buttons), m_index(index) {}
		ControlState GetState() const override;

	private:
		const unsigned int& m_buttons;
		const unsigned int m_index;
	};

	class Cursor : public Input
	{
	public:
		std::string GetName() const override;
		bool IsDetectable() override { return false; }
		Cursor(u8 index, bool positive, const float& cursor)
			: m_cursor(cursor), m_index(index), m_positive(positive) {}
		ControlState GetState() const override;

	private:
		const float& m_cursor;
		const u8     m_index;
		const bool   m_positive;
	};

public:
	void UpdateInput() override;

	KeyboardMouse(Window window);
	~KeyboardMouse();

	std::string GetName() const override;
	std::string GetSource() const override;
	int GetId() const override;

private:
	Window   m_window;
	Display* m_display;
	State    m_state;
};

}
}
