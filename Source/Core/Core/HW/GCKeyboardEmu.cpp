// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Core/HW/GCKeyboardEmu.h"
#include "InputCommon/KeyboardStatus.h"

static const u16 keys0_bitmasks[] =
{
	KEYMASK_HOME,
	KEYMASK_END,
	KEYMASK_PGUP,
	KEYMASK_PGDN,
	KEYMASK_SCROLLLOCK,
	KEYMASK_A,
	KEYMASK_B,
	KEYMASK_C,
	KEYMASK_D,
	KEYMASK_E,
	KEYMASK_F,
	KEYMASK_G,
	KEYMASK_H,
	KEYMASK_I,
	KEYMASK_J,
	KEYMASK_K
};
static const u16 keys1_bitmasks[] =
{
	KEYMASK_L,
	KEYMASK_M,
	KEYMASK_N,
	KEYMASK_O,
	KEYMASK_P,
	KEYMASK_Q,
	KEYMASK_R,
	KEYMASK_S,
	KEYMASK_T,
	KEYMASK_U,
	KEYMASK_V,
	KEYMASK_W,
	KEYMASK_X,
	KEYMASK_Y,
	KEYMASK_Z,
	KEYMASK_1
};
static const u16 keys2_bitmasks[] =
{
	KEYMASK_2,
	KEYMASK_3,
	KEYMASK_4,
	KEYMASK_5,
	KEYMASK_6,
	KEYMASK_7,
	KEYMASK_8,
	KEYMASK_9,
	KEYMASK_0,
	KEYMASK_MINUS,
	KEYMASK_PLUS,
	KEYMASK_PRINTSCR,
	KEYMASK_BRACE_OPEN,
	KEYMASK_BRACE_CLOSE,
	KEYMASK_COLON,
	KEYMASK_QUOTE
};
static const u16 keys3_bitmasks[] =
{
	KEYMASK_HASH,
	KEYMASK_COMMA,
	KEYMASK_PERIOD,
	KEYMASK_QUESTIONMARK,
	KEYMASK_INTERNATIONAL1,
	KEYMASK_F1,
	KEYMASK_F2,
	KEYMASK_F3,
	KEYMASK_F4,
	KEYMASK_F5,
	KEYMASK_F6,
	KEYMASK_F7,
	KEYMASK_F8,
	KEYMASK_F9,
	KEYMASK_F10,
	KEYMASK_F11
};
static const u16 keys4_bitmasks[] =
{
	KEYMASK_F12,
	KEYMASK_ESC,
	KEYMASK_INSERT,
	KEYMASK_DELETE,
	KEYMASK_TILDE,
	KEYMASK_BACKSPACE,
	KEYMASK_TAB,
	KEYMASK_CAPSLOCK,
	KEYMASK_LEFTSHIFT,
	KEYMASK_RIGHTSHIFT,
	KEYMASK_LEFTCONTROL,
	KEYMASK_RIGHTALT,
	KEYMASK_LEFTWINDOWS,
	KEYMASK_SPACE,
	KEYMASK_RIGHTWINDOWS,
	KEYMASK_MENU
};
static const u16 keys5_bitmasks[] =
{
	KEYMASK_LEFTARROW,
	KEYMASK_DOWNARROW,
	KEYMASK_UPARROW,
	KEYMASK_RIGHTARROW,
	KEYMASK_ENTER
};

static const char* const named_keys0[] =
{
	"HOME",
	"END",
	"PGUP",
	"PGDN",
	"SCR LK",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K"
};
static const char* const named_keys1[] =
{
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"1"
};
static const char* const named_keys2[] =
{
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
	"-",
	"`",
	"PRT SC",
	"'",
	"[",
	"EQUALS",
	"*"
};
static const char* const named_keys3[] =
{
	"]",
	",",
	".",
	"/",
	"\\",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11"
};
static const char* const named_keys4[] =
{
	"F12",
	"ESC",
	"INSERT",
	"DELETE",
	";",
	"BACKSPACE",
	"TAB",
	"CAPS LOCK",
	"L SHIFT",
	"R SHIFT",
	"L CTRL",
	"R ALT",
	"L WIN",
	"SPACE",
	"R WIN",
	"MENU"
};
static const char* const named_keys5[] =
{
	"LEFT",
	"DOWN",
	"UP",
	"RIGHT",
	"ENTER"
};

GCKeyboard::GCKeyboard(const unsigned int index) : m_index(index)
{
	// buttons
	groups.emplace_back(m_keys0x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys0)
		m_keys0x->controls.emplace_back(new ControlGroup::Input(key));

	groups.emplace_back(m_keys1x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys1)
		m_keys1x->controls.emplace_back(new ControlGroup::Input(key));

	groups.emplace_back(m_keys2x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys2)
		m_keys2x->controls.emplace_back(new ControlGroup::Input(key));

	groups.emplace_back(m_keys3x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys3)
		m_keys3x->controls.emplace_back(new ControlGroup::Input(key));

	groups.emplace_back(m_keys4x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys4)
		m_keys4x->controls.emplace_back(new ControlGroup::Input(key));

	groups.emplace_back(m_keys5x = new Buttons(_trans("Keys")));
	for (const char* key : named_keys5)
		m_keys5x->controls.emplace_back(new ControlGroup::Input(key));


	// options
	groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
	m_options->settings.emplace_back(new ControlGroup::BackgroundInputSetting(_trans("Background Input")));
	m_options->settings.emplace_back(new ControlGroup::IterateUI(_trans("Iterative Input")));
}

std::string GCKeyboard::GetName() const
{
	return std::string("GCKeyboard") + char('1' + m_index);
}

void GCKeyboard::GetInput(KeyboardStatus* const kb)
{
	m_keys0x->GetState(&kb->key0x, keys0_bitmasks);
	m_keys1x->GetState(&kb->key1x, keys1_bitmasks);
	m_keys2x->GetState(&kb->key2x, keys2_bitmasks);
	m_keys3x->GetState(&kb->key3x, keys3_bitmasks);
	m_keys4x->GetState(&kb->key4x, keys4_bitmasks);
	m_keys5x->GetState(&kb->key5x, keys5_bitmasks);
}

void GCKeyboard::LoadDefaults(const ControllerInterface& ciface)
{
	#define set_control(group, num, str)  (group)->controls[num]->control_ref->expression = (str)

	ControllerEmu::LoadDefaults(ciface);

	// Buttons
	set_control(m_keys0x, 5, "A");
	set_control(m_keys0x, 6, "B");
	set_control(m_keys0x, 7, "C");
	set_control(m_keys0x, 8, "D");
	set_control(m_keys0x, 9, "E");
	set_control(m_keys0x, 10, "F");
	set_control(m_keys0x, 11, "G");
	set_control(m_keys0x, 12, "H");
	set_control(m_keys0x, 13, "I");
	set_control(m_keys0x, 14, "J");
	set_control(m_keys0x, 15, "K");
	set_control(m_keys1x, 0, "L");
	set_control(m_keys1x, 1, "M");
	set_control(m_keys1x, 2, "N");
	set_control(m_keys1x, 3, "O");
	set_control(m_keys1x, 4, "P");
	set_control(m_keys1x, 5, "Q");
	set_control(m_keys1x, 6, "R");
	set_control(m_keys1x, 7, "S");
	set_control(m_keys1x, 8, "T");
	set_control(m_keys1x, 9, "U");
	set_control(m_keys1x, 10, "V");
	set_control(m_keys1x, 11, "W");
	set_control(m_keys1x, 12, "X");
	set_control(m_keys1x, 13, "Y");
	set_control(m_keys1x, 14, "Z");

	set_control(m_keys1x, 15, "1");
	set_control(m_keys2x, 0, "2");
	set_control(m_keys2x, 1, "3");
	set_control(m_keys2x, 2, "4");
	set_control(m_keys2x, 3, "5");
	set_control(m_keys2x, 4, "6");
	set_control(m_keys2x, 5, "7");
	set_control(m_keys2x, 6, "8");
	set_control(m_keys2x, 7, "9");
	set_control(m_keys2x, 8, "0");

	set_control(m_keys3x, 5, "F1");
	set_control(m_keys3x, 6, "F2");
	set_control(m_keys3x, 7, "F3");
	set_control(m_keys3x, 8, "F4");
	set_control(m_keys3x, 9, "F5");
	set_control(m_keys3x, 10, "F6");
	set_control(m_keys3x, 11, "F7");
	set_control(m_keys3x, 12, "F8");
	set_control(m_keys3x, 13, "F9");
	set_control(m_keys3x, 14, "F10");
	set_control(m_keys3x, 15, "F11");
	set_control(m_keys4x, 0, "F12");

#ifdef _WIN32
	set_control(m_keys0x, 0, "HOME");
	set_control(m_keys0x, 1, "END");
	set_control(m_keys0x, 2, "PRIOR");
	set_control(m_keys0x, 3, "NEXT");
	set_control(m_keys0x, 4, "SCROLL");

	set_control(m_keys2x, 9, "MINUS");
	set_control(m_keys2x, 10, "GRAVE");
	set_control(m_keys2x, 11, "SYSRQ");
	set_control(m_keys2x, 12, "APOSTROPHE");
	set_control(m_keys2x, 13, "LBRACKET");
	set_control(m_keys2x, 14, "EQUALS");
	set_control(m_keys2x, 15, "MULTIPLY");
	set_control(m_keys3x, 0, "RBRACKET");
	set_control(m_keys3x, 1, "COMMA");
	set_control(m_keys3x, 2, "PERIOD");
	set_control(m_keys3x, 3, "SLASH");
	set_control(m_keys3x, 4, "BACKSLASH");

	set_control(m_keys4x, 1, "ESCAPE");
	set_control(m_keys4x, 2, "INSERT");
	set_control(m_keys4x, 3, "DELETE");
	set_control(m_keys4x, 4, "SEMICOLON");
	set_control(m_keys4x, 5, "BACK");
	set_control(m_keys4x, 6, "TAB");
	set_control(m_keys4x, 7, "CAPITAL");
	set_control(m_keys4x, 8, "LSHIFT");
	set_control(m_keys4x, 9, "RSHIFT");
	set_control(m_keys4x, 10, "LCONTROL");
	set_control(m_keys4x, 11, "RMENU");
	set_control(m_keys4x, 12, "LWIN");
	set_control(m_keys4x, 13, "SPACE");
	set_control(m_keys4x, 14, "RWIN");
	set_control(m_keys4x, 15, "MENU");

	set_control(m_keys5x, 0, "LEFT");
	set_control(m_keys5x, 1, "DOWN");
	set_control(m_keys5x, 2, "UP");
	set_control(m_keys5x, 3, "RIGHT");
	set_control(m_keys5x, 4, "RETURN");
#elif __APPLE__
	set_control(m_keys0x, 0, "Home");
	set_control(m_keys0x, 1, "End");
	set_control(m_keys0x, 2, "Page Up");
	set_control(m_keys0x, 3, "Page Down");
	set_control(m_keys0x, 4, ""); // Scroll lock

	set_control(m_keys2x, 9, "-");
	set_control(m_keys2x, 10, "Paragraph");
	set_control(m_keys2x, 11, ""); // Print Scr
	set_control(m_keys2x, 12, "'");
	set_control(m_keys2x, 13, "[");
	set_control(m_keys2x, 14, "=");
	set_control(m_keys2x, 15, "Keypad *");
	set_control(m_keys3x, 0, "]");
	set_control(m_keys3x, 1, ",");
	set_control(m_keys3x, 2, ".");
	set_control(m_keys3x, 3, "/");
	set_control(m_keys3x, 4, "\\");

	set_control(m_keys4x, 1, "Escape");
	set_control(m_keys4x, 2, "Insert");
	set_control(m_keys4x, 3, "Delete");
	set_control(m_keys4x, 4, ";");
	set_control(m_keys4x, 5, "Backspace");
	set_control(m_keys4x, 6, "Tab");
	set_control(m_keys4x, 7, "Caps Lock");
	set_control(m_keys4x, 8, "Left Shift");
	set_control(m_keys4x, 9, "Right Shift");
	set_control(m_keys4x, 10, "Left Control");
	set_control(m_keys4x, 11, "Right Alt");
	set_control(m_keys4x, 12, "Left Command");
	set_control(m_keys4x, 13, "Space");
	set_control(m_keys4x, 14, "Right Command");
	set_control(m_keys4x, 15, ""); // Menu

	set_control(m_keys5x, 0, "Left Arrow");
	set_control(m_keys5x, 1, "Down Arrow");
	set_control(m_keys5x, 2, "Up Arrow");
	set_control(m_keys5x, 3, "Right Arrow");
	set_control(m_keys5x, 4, "Return");
#else // linux
	set_control(m_keys0x, 0, "Home");
	set_control(m_keys0x, 1, "End");
	set_control(m_keys0x, 2, "Prior");
	set_control(m_keys0x, 3, "Next");
	set_control(m_keys0x, 4, "Scroll_Lock");

	set_control(m_keys2x, 9, "minus");
	set_control(m_keys2x, 10, "grave");
	set_control(m_keys2x, 11, "Print");
	set_control(m_keys2x, 12, "apostrophe");
	set_control(m_keys2x, 13, "bracketleft");
	set_control(m_keys2x, 14, "equal");
	set_control(m_keys2x, 15, "KP_Multiply");
	set_control(m_keys3x, 0, "bracketright");
	set_control(m_keys3x, 1, "comma");
	set_control(m_keys3x, 2, "period");
	set_control(m_keys3x, 3, "slash");
	set_control(m_keys3x, 4, "backslash");

	set_control(m_keys4x, 1, "Escape");
	set_control(m_keys4x, 2, "Insert");
	set_control(m_keys4x, 3, "Delete");
	set_control(m_keys4x, 4, "semicolon");
	set_control(m_keys4x, 5, "BackSpace");
	set_control(m_keys4x, 6, "Tab");
	set_control(m_keys4x, 7, "Caps_Lock");
	set_control(m_keys4x, 8, "Shift_L");
	set_control(m_keys4x, 9, "Shift_R");
	set_control(m_keys4x, 10, "Control_L");
	set_control(m_keys4x, 11, "Alt_R");
	set_control(m_keys4x, 12, "Super_L");
	set_control(m_keys4x, 13, "space");
	set_control(m_keys4x, 14, "Super_R");
	set_control(m_keys4x, 15, "Menu");

	set_control(m_keys5x, 0, "Left");
	set_control(m_keys5x, 1, "Down");
	set_control(m_keys5x, 2, "Up");
	set_control(m_keys5x, 3, "Right");
	set_control(m_keys5x, 4, "Return");
#endif

}
