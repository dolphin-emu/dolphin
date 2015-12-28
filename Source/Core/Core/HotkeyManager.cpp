// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <vector>

#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/HotkeyManager.h"
#include "InputCommon/GCPadStatus.h"

const std::string hotkey_labels[] =
{
	_trans("Open"),
	_trans("Change Disc"),
	_trans("Refresh List"),

	_trans("Toggle Pause"),
	_trans("Stop"),
	_trans("Reset"),
	_trans("Frame Advance"),
	_trans("Frame Advance Decrease Speed"),
	_trans("Frame Advance Increase Speed"),
	_trans("Frame Advance Reset Speed"),

	_trans("Start Recording"),
	_trans("Play Recording"),
	_trans("Export Recording"),
	_trans("Read-only mode"),

	_trans("Toggle Fullscreen"),
	_trans("Take Screenshot"),
	_trans("Exit"),

	_trans("Connect Wiimote 1"),
	_trans("Connect Wiimote 2"),
	_trans("Connect Wiimote 3"),
	_trans("Connect Wiimote 4"),
	_trans("Connect Balance Board"),

	_trans("Volume Down"),
	_trans("Volume Up"),
	_trans("Volume Toggle Mute"),

	_trans("Increase IR"),
	_trans("Decrease IR"),

	_trans("Toggle Aspect Ratio"),
	_trans("Toggle EFB Copies"),
	_trans("Toggle Fog"),
	_trans("Toggle Frame limit"),
	_trans("Decrease Frame limit"),
	_trans("Increase Frame limit"),

	_trans("Freelook Decrease Speed"),
	_trans("Freelook Increase Speed"),
	_trans("Freelook Reset Speed"),
	_trans("Freelook Move Up"),
	_trans("Freelook Move Down"),
	_trans("Freelook Move Left"),
	_trans("Freelook Move Right"),
	_trans("Freelook Zoom In"),
	_trans("Freelook Zoom Out"),
	_trans("Freelook Reset"),

	_trans("Toggle 3D Side-by-side"),
	_trans("Toggle 3D Top-bottom"),
	_trans("Toggle 3D Anaglyph"),
	_trans("Toggle 3D Vision"),

	_trans("Decrease Depth"),
	_trans("Increase Depth"),
	_trans("Decrease Convergence"),
	_trans("Increase Convergence"),

	_trans("Load State Slot 1"),
	_trans("Load State Slot 2"),
	_trans("Load State Slot 3"),
	_trans("Load State Slot 4"),
	_trans("Load State Slot 5"),
	_trans("Load State Slot 6"),
	_trans("Load State Slot 7"),
	_trans("Load State Slot 8"),
	_trans("Load State Slot 9"),
	_trans("Load State Slot 10"),

	_trans("Save State Slot 1"),
	_trans("Save State Slot 2"),
	_trans("Save State Slot 3"),
	_trans("Save State Slot 4"),
	_trans("Save State Slot 5"),
	_trans("Save State Slot 6"),
	_trans("Save State Slot 7"),
	_trans("Save State Slot 8"),
	_trans("Save State Slot 9"),
	_trans("Save State Slot 10"),

	_trans("Select State Slot 1"),
	_trans("Select State Slot 2"),
	_trans("Select State Slot 3"),
	_trans("Select State Slot 4"),
	_trans("Select State Slot 5"),
	_trans("Select State Slot 6"),
	_trans("Select State Slot 7"),
	_trans("Select State Slot 8"),
	_trans("Select State Slot 9"),
	_trans("Select State Slot 10"),

	_trans("Save to selected slot"),
	_trans("Load from selected slot"),

	_trans("Load State Last 1"),
	_trans("Load State Last 2"),
	_trans("Load State Last 3"),
	_trans("Load State Last 4"),
	_trans("Load State Last 5"),
	_trans("Load State Last 6"),
	_trans("Load State Last 7"),
	_trans("Load State Last 8"),
	_trans("Load State Last 9"),
	_trans("Load State Last 10"),

	_trans("Save Oldest State"),
	_trans("Undo Load State"),
	_trans("Undo Save State"),
	_trans("Save State"),
	_trans("Load State"),
};
static_assert(NUM_HOTKEYS == sizeof(hotkey_labels) / sizeof(hotkey_labels[0]), "Wrong count of hotkey_labels");

namespace HotkeyManagerEmu
{

static u32 s_hotkeyDown[(NUM_HOTKEYS + 31) / 32];
static HotkeyStatus s_hotkey;
static bool s_enabled;

static InputConfig s_config("Hotkeys", _trans("Hotkeys"), "Hotkeys");

InputConfig* GetConfig()
{
	return &s_config;
}

void GetStatus()
{
	s_hotkey.err = PAD_ERR_NONE;

	// Get input
	static_cast<HotkeyManager*>(s_config.GetController(0))->GetInput(&s_hotkey);
}

bool IsEnabled()
{
	return s_enabled;
}

void Enable(bool enable_toggle)
{
	s_enabled = enable_toggle;
}

bool IsPressed(int Id, bool held)
{
	unsigned int set = Id / 32;
	unsigned int setKey = Id % 32;
	if (s_hotkey.button[set] & (1 << setKey))
	{
		bool pressed = !!(s_hotkeyDown[set] & (1 << setKey));
		s_hotkeyDown[set] |= (1 << setKey);
		if (!pressed || held)
			return true;
	}
	else
	{
		s_hotkeyDown[set] &= ~(1 << setKey);
	}

	return false;
}

void Initialize(void* const hwnd)
{
	if (s_config.ControllersNeedToBeCreated())
		s_config.CreateController<HotkeyManager>();

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);

	for (u32& key : s_hotkeyDown)
		key = 0;

	s_enabled = true;
}

void LoadConfig()
{
	s_config.LoadConfig(true);
}

void Shutdown()
{
	s_config.ClearControllers();

	g_controller_interface.Shutdown();
}

}

HotkeyManager::HotkeyManager()
{
	for (int key = 0; key < NUM_HOTKEYS; key++)
	{
		int set = key / 32;

		if (key % 32 == 0)
			groups.emplace_back(m_keys[set] = new Buttons(_trans("Keys")));

		m_keys[set]->controls.emplace_back(new ControlGroup::Input(hotkey_labels[key]));
	}

	groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
	m_options->settings.emplace_back(new ControlGroup::BackgroundInputSetting(_trans("Background Input")));
	m_options->settings.emplace_back(new ControlGroup::IterateUI(_trans("Iterative Input")));
}

HotkeyManager::~HotkeyManager()
{
}

std::string HotkeyManager::GetName() const
{
	return std::string("Hotkeys") + char('1' + 0);
}

void HotkeyManager::GetInput(HotkeyStatus* const kb)
{
	for (int set = 0; set < (NUM_HOTKEYS + 31) / 32; set++)
	{
		std::vector<u32> bitmasks;
		for (int key = 0; key < std::min(32, NUM_HOTKEYS - set * 32); key++)
			bitmasks.push_back(1 << key);

		kb->button[set] = 0;
		m_keys[set]->GetState(&kb->button[set], bitmasks.data());
	}
}

void HotkeyManager::LoadDefaults(const ControllerInterface& ciface)
{
	ControllerEmu::LoadDefaults(ciface);

#ifdef _WIN32
	const std::string NON = "(!(LMENU | RMENU) & !(LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
	const std::string ALT = "((LMENU | RMENU) & !(LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
	const std::string SHIFT = "(!(LMENU | RMENU) & (LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
	const std::string CTRL = "(!(LMENU | RMENU) & !(LSHIFT | RSHIFT) & (LCONTROL | RCONTROL))";
#else
	const std::string NON = "(!`Alt_L` & !(`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
	const std::string ALT = "(`Alt_L` & !(`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
	const std::string SHIFT = "(!`Alt_L` & (`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
	const std::string CTRL = "(!`Alt_L` & !(`Shift_L` | `Shift_R`) & (`Control_L` | `Control_R` ))";
#endif

	auto set_key_expression = [this](int index, const std::string& expression) {
		m_keys[index / 32]->controls[index % 32]->control_ref->expression = expression;
	};

	// General hotkeys
	set_key_expression(HK_OPEN, CTRL + " & O");
	set_key_expression(HK_PLAY_PAUSE, "`F10`");
#ifdef _WIN32
	set_key_expression(HK_STOP, "ESCAPE");
	set_key_expression(HK_FULLSCREEN, ALT + " & RETURN");
#else
	set_key_expression(HK_STOP, "Escape");
	set_key_expression(HK_FULLSCREEN, ALT + " & Return");
#endif
	set_key_expression(HK_SCREENSHOT, NON + " & `F9`");
	set_key_expression(HK_WIIMOTE1_CONNECT, ALT + " & `F5`");
	set_key_expression(HK_WIIMOTE2_CONNECT, ALT + " & `F6`");
	set_key_expression(HK_WIIMOTE3_CONNECT, ALT + " & `F7`");
	set_key_expression(HK_WIIMOTE4_CONNECT, ALT + " & `F8`");
	set_key_expression(HK_BALANCEBOARD_CONNECT, ALT + " & `F9`");
#ifdef _WIN32
	set_key_expression(HK_TOGGLE_THROTTLE, "TAB");
#else
	set_key_expression(HK_TOGGLE_THROTTLE, "Tab");
#endif

	// Freelook
	set_key_expression(HK_FREELOOK_DECREASE_SPEED, SHIFT + " & `1`");
	set_key_expression(HK_FREELOOK_INCREASE_SPEED, SHIFT + " & `2`");
	set_key_expression(HK_FREELOOK_RESET_SPEED, SHIFT + " & F");
	set_key_expression(HK_FREELOOK_UP, SHIFT + " & E");
	set_key_expression(HK_FREELOOK_DOWN, SHIFT + " & Q");
	set_key_expression(HK_FREELOOK_LEFT, SHIFT + " & A");
	set_key_expression(HK_FREELOOK_RIGHT, SHIFT + " & D");
	set_key_expression(HK_FREELOOK_ZOOM_IN, SHIFT + " & W");
	set_key_expression(HK_FREELOOK_ZOOM_OUT, SHIFT + " & S");
	set_key_expression(HK_FREELOOK_RESET, SHIFT + " & R");

	// Savestates
	for (int i = 0; i < 8; i++)
	{
		set_key_expression(HK_LOAD_STATE_SLOT_1 + i, StringFromFormat((NON + " & `F%d`").c_str(), i + 1));
		set_key_expression(HK_SAVE_STATE_SLOT_1 + i, StringFromFormat((SHIFT + " & `F%d`").c_str(), i + 1));
	}
	set_key_expression(HK_UNDO_LOAD_STATE, NON + " & `F12`");
	set_key_expression(HK_UNDO_SAVE_STATE, SHIFT + " & `F12`");
}
