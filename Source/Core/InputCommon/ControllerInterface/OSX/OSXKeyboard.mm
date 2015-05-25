// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>

#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <Cocoa/Cocoa.h>

#include "InputCommon/ControllerInterface/OSX/OSXKeyboard.h"

namespace ciface
{
namespace OSX
{

Keyboard::Keyboard(IOHIDDeviceRef device, std::string name, int index, void *window)
	: m_device(device)
	, m_device_name(name)
	, m_index(index)
{
	// This class should only recieve Keyboard or Keypad devices
	// Now, filter on just the buttons we can handle sanely
	NSDictionary *matchingElements = @{
		@kIOHIDElementTypeKey : [NSNumber numberWithInteger: kIOHIDElementTypeInput_Button],
		@kIOHIDElementMinKey  : [NSNumber numberWithInteger: 0],
		@kIOHIDElementMaxKey  : [NSNumber numberWithInteger: 1]
	};

	CFArrayRef elements = IOHIDDeviceCopyMatchingElements(m_device,
		(CFDictionaryRef)matchingElements, kIOHIDOptionsTypeNone);

	if (elements)
	{
		for (int i = 0; i < CFArrayGetCount(elements); i++)
		{
			IOHIDElementRef e =
			(IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
			//DeviceElementDebugPrint(e, nullptr);

			AddInput(new Key(e, m_device));
		}
		CFRelease(elements);
	}

	m_windowid = [[reinterpret_cast<NSView*>(window) window] windowNumber];

	// cursor, with a hax for-loop
	for (unsigned int i=0; i<4; ++i)
		AddInput(new Cursor(!!(i&2), (&m_cursor.x)[i/2], !!(i&1)));

	for (u8 i = 0; i < sizeof(m_mousebuttons) / sizeof(m_mousebuttons[0]); ++i)
		AddInput(new Button(i, m_mousebuttons[i]));
}

void Keyboard::UpdateInput()
{
	CGRect bounds = CGRectZero;
	uint32_t windowid[1] = { m_windowid };
	CFArrayRef windowArray = CFArrayCreate(nullptr, (const void **) windowid, 1, nullptr);
	CFArrayRef windowDescriptions = CGWindowListCreateDescriptionFromArray(windowArray);
	CFDictionaryRef windowDescription = (CFDictionaryRef) CFArrayGetValueAtIndex((CFArrayRef) windowDescriptions, 0);

	if (CFDictionaryContainsKey(windowDescription, kCGWindowBounds))
	{
		CFDictionaryRef boundsDictionary = (CFDictionaryRef) CFDictionaryGetValue(windowDescription, kCGWindowBounds);

		if (boundsDictionary != nullptr)
			CGRectMakeWithDictionaryRepresentation(boundsDictionary, &bounds);
	}

	CFRelease(windowDescriptions);
	CFRelease(windowArray);

	CGEventRef event = CGEventCreate(nil);
	CGPoint loc = CGEventGetLocation(event);
	CFRelease(event);

	loc.x -= bounds.origin.x;
	loc.y -= bounds.origin.y;
	m_cursor.x = loc.x / bounds.size.width * 2 - 1.0;
	m_cursor.y = loc.y / bounds.size.height * 2 - 1.0;

	m_mousebuttons[0] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft);
	m_mousebuttons[1] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight);
	m_mousebuttons[2] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter);
}

std::string Keyboard::GetName() const
{
	return m_device_name;
}

std::string Keyboard::GetSource() const
{
	return "Keyboard";
}

int Keyboard::GetId() const
{
	return m_index;
}

Keyboard::Key::Key(IOHIDElementRef element, IOHIDDeviceRef device)
	: m_element(element)
	, m_device(device)
{
	static const struct PrettyKeys
	{
		const uint32_t    code;
		const char *const name;
	} named_keys[] = {
		{ kHIDUsage_KeyboardA, "A" },
		{ kHIDUsage_KeyboardB, "B" },
		{ kHIDUsage_KeyboardC, "C" },
		{ kHIDUsage_KeyboardD, "D" },
		{ kHIDUsage_KeyboardE, "E" },
		{ kHIDUsage_KeyboardF, "F" },
		{ kHIDUsage_KeyboardG, "G" },
		{ kHIDUsage_KeyboardH, "H" },
		{ kHIDUsage_KeyboardI, "I" },
		{ kHIDUsage_KeyboardJ, "J" },
		{ kHIDUsage_KeyboardK, "K" },
		{ kHIDUsage_KeyboardL, "L" },
		{ kHIDUsage_KeyboardM, "M" },
		{ kHIDUsage_KeyboardN, "N" },
		{ kHIDUsage_KeyboardO, "O" },
		{ kHIDUsage_KeyboardP, "P" },
		{ kHIDUsage_KeyboardQ, "Q" },
		{ kHIDUsage_KeyboardR, "R" },
		{ kHIDUsage_KeyboardS, "S" },
		{ kHIDUsage_KeyboardT, "T" },
		{ kHIDUsage_KeyboardU, "U" },
		{ kHIDUsage_KeyboardV, "V" },
		{ kHIDUsage_KeyboardW, "W" },
		{ kHIDUsage_KeyboardX, "X" },
		{ kHIDUsage_KeyboardY, "Y" },
		{ kHIDUsage_KeyboardZ, "Z" },
		{ kHIDUsage_Keyboard1, "1" },
		{ kHIDUsage_Keyboard2, "2" },
		{ kHIDUsage_Keyboard3, "3" },
		{ kHIDUsage_Keyboard4, "4" },
		{ kHIDUsage_Keyboard5, "5" },
		{ kHIDUsage_Keyboard6, "6" },
		{ kHIDUsage_Keyboard7, "7" },
		{ kHIDUsage_Keyboard8, "8" },
		{ kHIDUsage_Keyboard9, "9" },
		{ kHIDUsage_Keyboard0, "0" },
		{ kHIDUsage_KeyboardReturnOrEnter, "Return" },
		{ kHIDUsage_KeyboardEscape, "Escape" },
		{ kHIDUsage_KeyboardDeleteOrBackspace, "Backspace" },
		{ kHIDUsage_KeyboardTab, "Tab" },
		{ kHIDUsage_KeyboardSpacebar, "Space" },
		{ kHIDUsage_KeyboardHyphen, "-" },
		{ kHIDUsage_KeyboardEqualSign, "=" },
		{ kHIDUsage_KeyboardOpenBracket, "[" },
		{ kHIDUsage_KeyboardCloseBracket, "]" },
		{ kHIDUsage_KeyboardBackslash, "\\" },
		{ kHIDUsage_KeyboardSemicolon, ";" },
		{ kHIDUsage_KeyboardQuote, "'" },
		{ kHIDUsage_KeyboardGraveAccentAndTilde, "Tilde" },
		{ kHIDUsage_KeyboardComma, "," },
		{ kHIDUsage_KeyboardPeriod, "." },
		{ kHIDUsage_KeyboardSlash, "/" },
		{ kHIDUsage_KeyboardCapsLock, "Caps Lock" },
		{ kHIDUsage_KeyboardF1, "F1" },
		{ kHIDUsage_KeyboardF2, "F2" },
		{ kHIDUsage_KeyboardF3, "F3" },
		{ kHIDUsage_KeyboardF4, "F4" },
		{ kHIDUsage_KeyboardF5, "F5" },
		{ kHIDUsage_KeyboardF6, "F6" },
		{ kHIDUsage_KeyboardF7, "F7" },
		{ kHIDUsage_KeyboardF8, "F8" },
		{ kHIDUsage_KeyboardF9, "F9" },
		{ kHIDUsage_KeyboardF10, "F10" },
		{ kHIDUsage_KeyboardF11, "F11" },
		{ kHIDUsage_KeyboardF12, "F12" },
		{ kHIDUsage_KeyboardInsert, "Insert" },
		{ kHIDUsage_KeyboardHome, "Home" },
		{ kHIDUsage_KeyboardPageUp, "Page Up" },
		{ kHIDUsage_KeyboardDeleteForward, "Delete" },
		{ kHIDUsage_KeyboardEnd, "End" },
		{ kHIDUsage_KeyboardPageDown, "Page Down" },
		{ kHIDUsage_KeyboardRightArrow, "Right Arrow" },
		{ kHIDUsage_KeyboardLeftArrow, "Left Arrow" },
		{ kHIDUsage_KeyboardDownArrow, "Down Arrow" },
		{ kHIDUsage_KeyboardUpArrow, "Up Arrow" },
		{ kHIDUsage_KeypadSlash, "Keypad /" },
		{ kHIDUsage_KeypadAsterisk, "Keypad *" },
		{ kHIDUsage_KeypadHyphen, "Keypad -" },
		{ kHIDUsage_KeypadPlus, "Keypad +" },
		{ kHIDUsage_KeypadEnter, "Keypad Enter" },
		{ kHIDUsage_Keypad1, "Keypad 1" },
		{ kHIDUsage_Keypad2, "Keypad 2" },
		{ kHIDUsage_Keypad3, "Keypad 3" },
		{ kHIDUsage_Keypad4, "Keypad 4" },
		{ kHIDUsage_Keypad5, "Keypad 5" },
		{ kHIDUsage_Keypad6, "Keypad 6" },
		{ kHIDUsage_Keypad7, "Keypad 7" },
		{ kHIDUsage_Keypad8, "Keypad 8" },
		{ kHIDUsage_Keypad9, "Keypad 9" },
		{ kHIDUsage_Keypad0, "Keypad 0" },
		{ kHIDUsage_KeypadPeriod, "Keypad ." },
		{ kHIDUsage_KeyboardNonUSBackslash, "Paragraph" },
		{ kHIDUsage_KeypadEqualSign, "Keypad =" },
		{ kHIDUsage_KeypadComma, "Keypad ," },
		{ kHIDUsage_KeyboardLeftControl, "Left Control" },
		{ kHIDUsage_KeyboardLeftShift, "Left Shift" },
		{ kHIDUsage_KeyboardLeftAlt, "Left Alt" },
		{ kHIDUsage_KeyboardLeftGUI, "Left Command" },
		{ kHIDUsage_KeyboardRightControl, "Right Control" },
		{ kHIDUsage_KeyboardRightShift, "Right Shift" },
		{ kHIDUsage_KeyboardRightAlt, "Right Alt" },
		{ kHIDUsage_KeyboardRightGUI, "Right Command" },
		{ 184, "Eject" },
	};

	const uint32_t keycode = IOHIDElementGetUsage(m_element);
	for (auto & named_key : named_keys)
	{
		if (named_key.code == keycode)
		{
			m_name = named_key.name;
			return;
		}
	}

	std::stringstream ss;
	ss << "Key " << keycode;
	m_name = ss.str();
}

ControlState Keyboard::Key::GetState() const
{
	IOHIDValueRef value;

	if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
		return IOHIDValueGetIntegerValue(value);
	else
		return 0;
}

ControlState Keyboard::Cursor::GetState() const
{
	return std::max(0.0, ControlState(m_axis) / (m_positive ? 1.0 : -1.0));
}

ControlState Keyboard::Button::GetState() const
{
	return (m_button != 0);
}

std::string Keyboard::Cursor::GetName() const
{
	static char tmpstr[] = "Cursor ..";
	tmpstr[7] = (char)('X' + m_index);
	tmpstr[8] = (m_positive ? '+' : '-');
	return tmpstr;
}

std::string Keyboard::Button::GetName() const
{
	return std::string("Click ") + char('0' + m_index);
}

std::string Keyboard::Key::GetName() const
{
	return m_name;
}


}
}
