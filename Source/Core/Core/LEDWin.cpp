// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <string>

// from Externals/Logitech
#include "LogitechLEDLib.h"

#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/LED.h"
#include "Core/HW/GCPad.h"
#include "InputCommon/ControllerEmu.h"
#include "InputCommon/InputConfig.h"

namespace LED
{

// color table; index is used in the color mapping below!
static const struct
{
	u8 red;
	u8 green;
	u8 blue;
} s_colors[] =
{
	// 0: white (default for keys that have no other mapping)
	{ 0xFF, 0xFF, 0xFF },
	// 1: gray (D-Pad)
	{ 0x95, 0x95, 0x95 },
	// 2: green (A-Button)
	{ 0x00, 0xFF, 0x00 },
	// 3: red (B-Button)
	{ 0xFF, 0x00, 0x00 },
	// 4: yellow (C-Stick)
	{ 0xFF, 0xFF, 0x00 },
	// 5: purple (Z-Trigger)
	{ 0x42, 0x00, 0xFF },
	// 6: blue (Main Stick, since it looks nicer than white)
	{ 0x00, 0xAA, 0xFF },
	// 7: brown (L/R Triggers, to distinguish them from other keys)
	{ 0x80, 0x35, 0x00 },
};

// overrides for specific named keys.
// anything not in this list will use s_colors[0] as default color.
// key is derived from a combination of
// ControllerEmu::ControlGroup::name and ControllerEmu::ControlGroup::Control::name.
// an example would be "Buttons/A" or "C-Stick/Up"
static std::map<std::string, int> s_named_key_colors =
{
	{ "Buttons/A", 2 },
	{ "Buttons/B", 3 },
	{ "Buttons/Z", 5 },
	{ "D-Pad/Left", 1 },
	{ "D-Pad/Up", 1 },
	{ "D-Pad/Right", 1 },
	{ "D-Pad/Down", 1 },
	{ "C-Stick/Left", 4 },
	{ "C-Stick/Up", 4 },
	{ "C-Stick/Right", 4 },
	{ "C-Stick/Down", 4 },
	{ "C-Stick/Modifier", 4 },
	{ "Main Stick/Left", 6 },
	{ "Main Stick/Up", 6 },
	{ "Main Stick/Right", 6 },
	{ "Main Stick/Down", 6 },
	{ "Main Stick/Modifier", 6 },
	{ "Triggers/L", 7 },
	{ "Triggers/R", 7 },
	{ "Triggers/L-Analog", 7 },
	{ "Triggers/R-Analog", 7 },
};

// mapping from our keys to Logitech KeyName (sadly, they are not contiguous)
static std::map<std::string, LogiLed::KeyName> s_keynames =
{
	// alpha keys
	{ "A", LogiLed::KeyName::A },
	{ "B", LogiLed::KeyName::B },
	{ "C", LogiLed::KeyName::C },
	{ "D", LogiLed::KeyName::D },
	{ "E", LogiLed::KeyName::E },
	{ "F", LogiLed::KeyName::F },
	{ "G", LogiLed::KeyName::G },
	{ "H", LogiLed::KeyName::H },
	{ "I", LogiLed::KeyName::I },
	{ "J", LogiLed::KeyName::J },
	{ "K", LogiLed::KeyName::K },
	{ "L", LogiLed::KeyName::L },
	{ "M", LogiLed::KeyName::M },
	{ "N", LogiLed::KeyName::N },
	{ "O", LogiLed::KeyName::O },
	{ "P", LogiLed::KeyName::P },
	{ "Q", LogiLed::KeyName::Q },
	{ "R", LogiLed::KeyName::R },
	{ "S", LogiLed::KeyName::S },
	{ "T", LogiLed::KeyName::T },
	{ "U", LogiLed::KeyName::U },
	{ "V", LogiLed::KeyName::V },
	{ "W", LogiLed::KeyName::W },
	{ "X", LogiLed::KeyName::X },
	{ "Y", LogiLed::KeyName::Y },
	{ "Z", LogiLed::KeyName::Z },
	// number keys
	{ "1", LogiLed::KeyName::ONE },
	{ "2", LogiLed::KeyName::TWO },
	{ "3", LogiLed::KeyName::THREE },
	{ "4", LogiLed::KeyName::FOUR },
	{ "5", LogiLed::KeyName::FIVE },
	{ "6", LogiLed::KeyName::SIX },
	{ "7", LogiLed::KeyName::SEVEN },
	{ "8", LogiLed::KeyName::EIGHT },
	{ "9", LogiLed::KeyName::NINE },
	// arrow keys
	{ "LEFT", LogiLed::KeyName::ARROW_LEFT },
	{ "UP", LogiLed::KeyName::ARROW_UP },
	{ "RIGHT", LogiLed::KeyName::ARROW_RIGHT },
	{ "DOWN", LogiLed::KeyName::ARROW_DOWN },
	// numpad keys
	{ "NUMLOCK", LogiLed::KeyName::NUM_LOCK },
	{ "NUMPAD1", LogiLed::KeyName::NUM_ONE },
	{ "NUMPAD2", LogiLed::KeyName::NUM_TWO },
	{ "NUMPAD3", LogiLed::KeyName::NUM_THREE },
	{ "NUMPAD4", LogiLed::KeyName::NUM_FOUR },
	{ "NUMPAD5", LogiLed::KeyName::NUM_FIVE },
	{ "NUMPAD6", LogiLed::KeyName::NUM_SIX },
	{ "NUMPAD7", LogiLed::KeyName::NUM_SEVEN },
	{ "NUMPAD8", LogiLed::KeyName::NUM_EIGHT },
	{ "NUMPAD9", LogiLed::KeyName::NUM_NINE },
	{ "NUMPADENTER", LogiLed::KeyName::NUM_ENTER },
	{ "ADD", LogiLed::KeyName::NUM_PLUS },
	{ "SUBTRACT", LogiLed::KeyName::NUM_MINUS },
	{ "MULTIPLY", LogiLed::KeyName::NUM_ASTERISK },
	{ "DIVIDE", LogiLed::KeyName::NUM_SLASH },
	{ "DECIMAL", LogiLed::KeyName::NUM_PERIOD },
	// punctation etc.
	{ "COMMA", LogiLed::KeyName::COMMA },
	{ "PERIOD", LogiLed::KeyName::PERIOD },
	{ "SLASH", LogiLed::KeyName::FORWARD_SLASH },
	{ "BACKSLASH", LogiLed::KeyName::BACKSLASH },
	{ "SEMICOLON", LogiLed::KeyName::SEMICOLON },
	{ "APOSTROPHE", LogiLed::KeyName::APOSTROPHE },
	{ "LBRACKET", LogiLed::KeyName::OPEN_BRACKET },
	{ "RBRACKET", LogiLed::KeyName::CLOSE_BRACKET },
	{ "GRAVE", LogiLed::KeyName::TILDE },
	{ "MINUS", LogiLed::KeyName::MINUS },
	{ "EQUALS", LogiLed::KeyName::EQUALS },
	// big keys
	{ "RETURN", LogiLed::KeyName::ENTER },
	{ "SPACE", LogiLed::KeyName::SPACE },
	{ "BACK", LogiLed::KeyName::BACKSPACE },
	{ "TAB", LogiLed::KeyName::TAB },
	{ "LSHIFT", LogiLed::KeyName::LEFT_SHIFT },
	{ "RSHIFT", LogiLed::KeyName::RIGHT_SHIFT },
	{ "LCONTROL", LogiLed::KeyName::LEFT_CONTROL },
	{ "RCONTROL", LogiLed::KeyName::RIGHT_CONTROL },
	{ "LMENU", LogiLed::KeyName::LEFT_ALT },
	{ "RMENU", LogiLed::KeyName::RIGHT_ALT },
	{ "LWIN", LogiLed::KeyName::LEFT_WINDOWS },
	{ "RWIN", LogiLed::KeyName::RIGHT_WINDOWS },
	{ "APPS", LogiLed::KeyName::APPLICATION_SELECT },
	// F-keys
	{ "F1", LogiLed::KeyName::F1 },
	{ "F2", LogiLed::KeyName::F2 },
	{ "F3", LogiLed::KeyName::F3 },
	{ "F4", LogiLed::KeyName::F4 },
	{ "F5", LogiLed::KeyName::F5 },
	{ "F6", LogiLed::KeyName::F6 },
	{ "F7", LogiLed::KeyName::F7 },
	{ "F8", LogiLed::KeyName::F8 },
	{ "F9", LogiLed::KeyName::F9 },
	{ "F10", LogiLed::KeyName::F10 },
	{ "F11", LogiLed::KeyName::F11 },
	{ "F12", LogiLed::KeyName::F12 },
	// other keys
	{ "ESCAPE", LogiLed::KeyName::ESC },
	{ "INSERT", LogiLed::KeyName::INSERT },
	{ "DELETE", LogiLed::KeyName::KEYBOARD_DELETE },
	{ "HOME", LogiLed::KeyName::HOME },
	{ "END", LogiLed::KeyName::END },
	{ "PRIOR", LogiLed::KeyName::PAGE_UP },
	{ "NEXT", LogiLed::KeyName::PAGE_DOWN },
	{ "SYSRQ", LogiLed::KeyName::PRINT_SCREEN },
	{ "SCROLL", LogiLed::KeyName::SCROLL_LOCK },
	{ "PAUSE", LogiLed::KeyName::PAUSE_BREAK },
};

// are we ready to call LogiLed* functions?
static bool s_initialized = false;

void Initialize()
{
	// see if we can init the Logitech Gaming LED SDK. If not, just silently drop out.
	// this needs to be done before calling any LogiLed* functions; and some of them
	// even require some delay between calls in order to register properly.
	s_initialized = LogiLedInit();

	if (s_initialized)
	{
		int major;
		int minor;
		int build;
		if (LogiLedGetSdkVersion(&major, &minor, &build))
			NOTICE_LOG(PAD, "Using Logitech Gaming LED SDK v%d.%d.%d", major, minor, build);
	}
}

void Start()
{
	if (!s_initialized)
		return;

	// remember what the current lighting looks like.
	// this actually returns success, but we'll just overwrite the layout regardless
	// (since it will be reset at latest when shutdown is called).
	if (!LogiLedSaveCurrentLighting())
		WARN_LOG(PAD, "Failed to save current Lighting state");

	// it seems that LogiLedSaveCurrentLighting has a slight delay and needs to finish before
	// doing anything else with it. Otherwise it might store the new state instead of the old
	// one if we call LogiLedSetLighting(ForKeyName) too quickly.
	// 20ms seemed to work fine during testing, 10ms is too low. Values up to 50 or even 100ms
	// are not really noticable and could be used in case 20 doesn't work everywhere.
	Common::SleepCurrentThread(20);

	// only talk to per-key devices for now; others won't make much sense
	// due to their limited possibilities of displaying control state.
	LogiLedSetTargetDevice(LOGI_DEVICETYPE_PERKEY_RGB);
	// black out all keys initially
	LogiLedSetLighting(0, 0, 0);

	InputConfig* const pad_plugin = Pad::GetConfig();
	// grab the Player 1 controller (TODO: what about other local players?)
	ControllerEmu* const controller = pad_plugin->controllers[0];

	for (auto& group : controller->groups)
	{
		for (auto& control : group->controls)
		{
			// we can only handle controls that refer to exactly one key (for now)
			// technically, we could handle CTRL/SHIFT/ALT combinations, but that would
			// require more work (ie. switching the scheme on key-press etc.)
			if (control->control_ref->BoundCount() == 1)
			{
				// find the appropriate color index for the given control
				std::string control_name = group->name + '/' + control->name;
				const int color_index = std::max(s_named_key_colors[control_name], 0);

				// translate the key into the Logitech equivalent
				std::string& key = control->control_ref->expression;
				const LogiLed::KeyName key_name = s_keynames[key];

				// set the given key's color if it is a valid one. ESC is the lowest KeyName there is.
				if (key_name >= LogiLed::KeyName::ESC)
					LogiLedSetLightingForKeyWithKeyName(key_name,
						// s_colors has 0..255, but this function expects a percentage
						s_colors[color_index].red * 100 / 0xFF,
						s_colors[color_index].green * 100 / 0xFF,
						s_colors[color_index].blue * 100 / 0xFF);
			}
		}
	}
}

void Stop()
{
	if (!s_initialized)
		return;

	// restore the layout we had before Start
	if (!LogiLedRestoreLighting())
		WARN_LOG(PAD, "Failed to restore Lighting state");
}

void Shutdown()
{
	if (!s_initialized)
		return;

	LogiLedShutdown();
}

}
