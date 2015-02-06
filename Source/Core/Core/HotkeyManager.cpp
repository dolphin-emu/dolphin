// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/HotkeyManager.h"

static const u32 hotkey_bitmasks[] =
{
	1 << 0,
	1 << 1,
	1 << 2,
	1 << 3,
	1 << 4,
	1 << 5,
	1 << 6,
	1 << 7,
	1 << 8,
	1 << 9,
	1 << 10,
	1 << 11,
	1 << 12,
	1 << 13,
	1 << 14,
	1 << 15,
	1 << 16,
	1 << 17,
	1 << 18,
	1 << 19,
	1 << 20,
	1 << 21,
	1 << 22,
	1 << 23,
	1 << 24,
	1 << 25,
	1 << 26,
	1 << 27,
	1 << 28,
	1 << 29,
	1 << 30,
	1u << 31u
};

const std::string hotkey_labels[] =
{
	_trans("Open"),
	_trans("Change Disc"),
	_trans("Refresh List"),

	_trans("Toggle Pause"),
	_trans("Stop"),
	_trans("Reset"),
	_trans("Frame Advance"),

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

	_trans("Volume Up"),
	_trans("Volume Down"),
	_trans("Volume Toggle Mute"),

	_trans("Toggle IR"),
	_trans("Toggle Aspect Ratio"),
	_trans("Toggle EFB Copies"),
	_trans("Toggle Fog"),
	_trans("Toggle Frame limit"),
	_trans("Increase Frame limit"),
	_trans("Decrease Frame limit"),

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

	_trans("Increase Depth"),
	_trans("Decrease Depth"),
	_trans("Increase Convergence"),
	_trans("Decrease Convergence"),

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

	_trans("Save Oldest State"),
	_trans("Undo Load State"),
	_trans("Undo Save State"),
	_trans("Save State"),
	_trans("Load State"),
};

const int num_hotkeys = (sizeof(hotkey_labels) / sizeof(hotkey_labels[0]));

namespace HotkeyManagerEmu
{

static u32 hotkeyDown[6];

static InputConfig s_config("Hotkeys", _trans("Hotkeys"), "Hotkeys");
InputConfig* GetConfig()
{
	return &s_config;
}

void GetStatus(u8 _port, HotkeyStatus* _pHotkeyStatus)
{
	memset(_pHotkeyStatus, 0, sizeof(*_pHotkeyStatus));
	_pHotkeyStatus->err = PAD_ERR_NONE;

	std::unique_lock<std::recursive_mutex> lk(s_config.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
	{
		// if gui has lock (messing with controls), skip this input cycle
		for (int i = 0; i < 6; i++)
			_pHotkeyStatus->button[i] = 0;

		return;
	}

	// get input
	((HotkeyManager*)s_config.controllers[_port])->GetInput(_pHotkeyStatus);
}

bool IsPressed(int Id, bool held)
{
	HotkeyStatus hotkey;
	memset(&hotkey, 0, sizeof(hotkey));
	GetStatus(0, &hotkey);
	unsigned int set = Id / 32;
	unsigned int setKey = Id % 32;
	if (hotkey.button[set] & (1 << setKey))
	{
		hotkeyDown[set] |= (1 << setKey);
		if (held)
			return true;
	}
	else
	{
		bool pressed = !!(hotkeyDown[set] & (1 << setKey));
		hotkeyDown[set] &= ~(1 << setKey);
		if (pressed)
			return true;
	}

	return false;
}

void Initialize(void* const hwnd)
{
	if (s_config.controllers.empty())
		s_config.controllers.push_back(new HotkeyManager());

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);

	for (unsigned int i = 0; i < 6; ++i)
		hotkeyDown[i] = 0;
}

void Shutdown()
{
	std::vector<ControllerEmu*>::const_iterator
		i = s_config.controllers.begin(),
		e = s_config.controllers.end();
	for (; i != e; ++i)
		delete *i;
	s_config.controllers.clear();

	g_controller_interface.Shutdown();
}

}

HotkeyManager::HotkeyManager()
{
	for (int set = 0; set < 6; set++)
	{
		// buttons
		if ((set * 32) < num_hotkeys)
			groups.emplace_back(m_keys[set] = new Buttons(_trans("Keys")));

		for (int key = 0; key < 32; key++)
		{
			if ((set * 32 + key) < num_hotkeys)
			{
				m_keys[set]->controls.emplace_back(new ControlGroup::Input(hotkey_labels[set * 32 + key]));
			}
		}
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
	for (int set = 0; set < 6; set++)
		if ((set * 32) < num_hotkeys)
			m_keys[set]->GetState(&kb->button[set], hotkey_bitmasks);
}

void HotkeyManager::LoadDefaults(const ControllerInterface& ciface)
{
#define set_control(group, num, str)  (group)->controls[num]->control_ref->expression = (str)

	ControllerEmu::LoadDefaults(ciface);

	// Buttons
#ifdef _WIN32
	set_control(m_keys[0], 0, "(LCONTROL | RCONTROL) & L"); // Open
	set_control(m_keys[0], 1, ""); // ChangeDisc
	set_control(m_keys[0], 2, ""); // RefreshList
	set_control(m_keys[0], 3, "F10"); // PlayPause
	set_control(m_keys[0], 4, "ESCAPE"); // Stop
	set_control(m_keys[0], 5, ""); // Reset
	set_control(m_keys[0], 6, ""); // FrameAdvance
	set_control(m_keys[0], 7, ""); // StartRecording
	set_control(m_keys[0], 8, ""); // PlayRecording
	set_control(m_keys[0], 9, ""); // ExportRecording
	set_control(m_keys[0], 10, ""); // Readonlymode
	set_control(m_keys[0], 11, "(LMENU | RMENU) & RETURN"); // ToggleFullscreen
	set_control(m_keys[0], 12, "`F9` & !(LMENU | RMENU)"); // Screenshot
	set_control(m_keys[0], 13, ""); // Exit
	set_control(m_keys[0], 14, "(LMENU | RMENU) & `F5`"); // Wiimote1Connect
	set_control(m_keys[0], 15, "(LMENU | RMENU) & `F6`"); // Wiimote2Connect
	set_control(m_keys[0], 16, "(LMENU | RMENU) & `F7`"); // Wiimote3Connect
	set_control(m_keys[0], 17, "(LMENU | RMENU) & `F8`"); // Wiimote4Connect
	set_control(m_keys[0], 18, "(LMENU | RMENU) & `F9`"); // BalanceBoardConnect
	set_control(m_keys[0], 19, ""); // VolumeDown
	set_control(m_keys[0], 20, ""); // VolumeUp
	set_control(m_keys[0], 21, ""); // VolumeToggleMute
	set_control(m_keys[0], 22, ""); // ToggleIR
	set_control(m_keys[0], 23, ""); // ToggleAspectRatio
	set_control(m_keys[0], 24, ""); // ToggleEFBCopies
	set_control(m_keys[0], 25, ""); // ToggleFog
	set_control(m_keys[0], 26, "TAB"); // ToggleThrottle
	set_control(m_keys[0], 27, ""); // DecreaseFrameLimit
	set_control(m_keys[0], 28, ""); // IncreaseFrameLimit
	set_control(m_keys[0], 29, "1"); // FreelookDecreaseSpeed
	set_control(m_keys[0], 30, "2"); // FreelookIncreaseSpeed
	set_control(m_keys[0], 31, "F"); // FreelookResetSpeed
	set_control(m_keys[1], 0, "E"); // FreelookUp
	set_control(m_keys[1], 1, "Q"); // FreelookDown
	set_control(m_keys[1], 2, "A"); // FreelookLeft
	set_control(m_keys[1], 3, "D"); // FreelookRight
	set_control(m_keys[1], 4, "W"); // FreelookZoomIn
	set_control(m_keys[1], 5, "S"); // FreelookZoomOut
	set_control(m_keys[1], 6, "R"); // FreelookReset
	set_control(m_keys[1], 7, ""); // DecreaseDepth
	set_control(m_keys[1], 8, ""); // IncreaseDepth
	set_control(m_keys[1], 9, ""); // DecreaseConvergence
	set_control(m_keys[1], 10, ""); // IncreaseConvergence
	set_control(m_keys[1], 11, "`F1` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot1
	set_control(m_keys[1], 12, "`F2` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot2
	set_control(m_keys[1], 13, "`F3` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot3
	set_control(m_keys[1], 14, "`F4` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot4
	set_control(m_keys[1], 15, "`F5` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot5
	set_control(m_keys[1], 16, "`F6` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot6
	set_control(m_keys[1], 17, "`F7` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot7
	set_control(m_keys[1], 18, "`F8` & !(LSHIFT | RSHIFT) & !(LMENU | RMENU)"); // LoadStateSlot8
	set_control(m_keys[1], 19, ""); // LoadStateSlot9
	set_control(m_keys[1], 20, ""); // LoadStateSlot10
	set_control(m_keys[1], 21, "(LSHIFT | RSHIFT) & `F1`"); // SaveStateSlot1
	set_control(m_keys[1], 22, "(LSHIFT | RSHIFT) & `F2`"); // SaveStateSlot2
	set_control(m_keys[1], 23, "(LSHIFT | RSHIFT) & `F3`"); // SaveStateSlot3
	set_control(m_keys[1], 24, "(LSHIFT | RSHIFT) & `F4`"); // SaveStateSlot4
	set_control(m_keys[1], 25, "(LSHIFT | RSHIFT) & `F5`"); // SaveStateSlot5
	set_control(m_keys[1], 26, "(LSHIFT | RSHIFT) & `F6`"); // SaveStateSlot6
	set_control(m_keys[1], 27, "(LSHIFT | RSHIFT) & `F7`"); // SaveStateSlot7
	set_control(m_keys[1], 28, "(LSHIFT | RSHIFT) & `F8`"); // SaveStateSlot8
	set_control(m_keys[1], 29, ""); // SaveStateSlot9
	set_control(m_keys[1], 30, ""); // SaveStateSlot10
	set_control(m_keys[1], 31, ""); // SelectStateSlot1
	set_control(m_keys[2], 0, ""); // SelectStateSlot2
	set_control(m_keys[2], 1, ""); // SelectStateSlot3
	set_control(m_keys[2], 2, ""); // SelectStateSlot4
	set_control(m_keys[2], 3, ""); // SelectStateSlot5
	set_control(m_keys[2], 4, ""); // SelectStateSlot6
	set_control(m_keys[2], 5, ""); // SelectStateSlot7
	set_control(m_keys[2], 6, ""); // SelectStateSlot8
	set_control(m_keys[2], 7, ""); // SelectStateSlot9
	set_control(m_keys[2], 8, ""); // SelectStateSlot10
	set_control(m_keys[2], 9, ""); // SaveSelectedSlot
	set_control(m_keys[2], 10, ""); // LoadSelectedSlot
	set_control(m_keys[2], 11, ""); // LoadLastState1
	set_control(m_keys[2], 12, ""); // LoadLastState2
	set_control(m_keys[2], 13, ""); // LoadLastState3
	set_control(m_keys[2], 14, ""); // LoadLastState4
	set_control(m_keys[2], 15, ""); // LoadLastState5
	set_control(m_keys[2], 16, ""); // LoadLastState6
	set_control(m_keys[2], 17, ""); // LoadLastState7
	set_control(m_keys[2], 18, ""); // LoadLastState8
	set_control(m_keys[2], 19, ""); // SaveFirstState
	set_control(m_keys[2], 20, "`F12` & !(LSHIFT | RSHIFT)"); // UndoLoadState
	set_control(m_keys[2], 21, "(LSHIFT | RSHIFT) & `F12`"); // UndoSaveState
	set_control(m_keys[2], 22, ""); // SaveStateFile
	set_control(m_keys[2], 23, ""); // LoadStateFile
#elif __APPLE__
	set_control(m_keys[0], 0, "(`Left Command` | `Right Command`) & `O`"); // Open
	set_control(m_keys[0], 1, ""); // ChangeDisc
	set_control(m_keys[0], 2, ""); // RefreshList
	set_control(m_keys[0], 3, "(`Left Command` | `Right Command`) & `P`"); // PlayPause
	set_control(m_keys[0], 4, "(`Left Command` | `Right Command`) & `W`"); // Stop
	set_control(m_keys[0], 5, ""); // Reset
	set_control(m_keys[0], 6, ""); // FrameAdvance
	set_control(m_keys[0], 7, ""); // StartRecording
	set_control(m_keys[0], 8, ""); // PlayRecording
	set_control(m_keys[0], 9, ""); // ExportRecording
	set_control(m_keys[0], 10, ""); // Readonlymode
	set_control(m_keys[0], 11, "(`Left Command` | `Right Command`) & `F`"); // ToggleFullscreen
	set_control(m_keys[0], 12, "(`Left Command` | `Right Command`) & `S`"); // Screenshot
	set_control(m_keys[0], 13, ""); // Exit
	set_control(m_keys[0], 14, "(`Left Command` | `Right Command`) & `1`"); // Wiimote1Connect
	set_control(m_keys[0], 15, "(`Left Command` | `Right Command`) & `2`"); // Wiimote2Connect
	set_control(m_keys[0], 16, "(`Left Command` | `Right Command`) & `3`"); // Wiimote3Connect
	set_control(m_keys[0], 17, "(`Left Command` | `Right Command`) & `4`"); // Wiimote4Connect
	set_control(m_keys[0], 18, "(`Left Command` | `Right Command`) & `5`"); // BalanceBoardConnect
	set_control(m_keys[0], 19, ""); // VolumeDown
	set_control(m_keys[0], 20, ""); // VolumeUp
	set_control(m_keys[0], 21, ""); // VolumeToggleMute
	set_control(m_keys[0], 22, ""); // ToggleIR
	set_control(m_keys[0], 23, ""); // ToggleAspectRatio
	set_control(m_keys[0], 24, ""); // ToggleEFBCopies
	set_control(m_keys[0], 25, ""); // ToggleFog
	set_control(m_keys[0], 26, "Tab"); // ToggleThrottle
	set_control(m_keys[0], 27, ""); // DecreaseFrameLimit
	set_control(m_keys[0], 28, ""); // IncreaseFrameLimit
	set_control(m_keys[0], 29, "`1` & !(`Left Command` | `Right Command`)"); // FreelookDecreaseSpeed
	set_control(m_keys[0], 30, "`2` & !(`Left Command` | `Right Command`)"); // FreelookIncreaseSpeed
	set_control(m_keys[0], 31, "`F` & !(`Left Command` | `Right Command`)"); // FreelookResetSpeed
	set_control(m_keys[1], 0, "`E` & !(`Left Command` | `Right Command`)"); // FreelookUp
	set_control(m_keys[1], 1, "`Q` & !(`Left Command` | `Right Command`)"); // FreelookDown
	set_control(m_keys[1], 2, "`A` & !(`Left Command` | `Right Command`)"); // FreelookLeft
	set_control(m_keys[1], 3, "`D` & !(`Left Command` | `Right Command`)"); // FreelookRight
	set_control(m_keys[1], 4, "`W` & !(`Left Command` | `Right Command`)"); // FreelookZoomIn
	set_control(m_keys[1], 5, "`S` & !(`Left Command` | `Right Command`)"); // FreelookZoomOut
	set_control(m_keys[1], 6, "`R` & !(`Left Command` | `Right Command`)"); // FreelookReset
	set_control(m_keys[1], 7, ""); // DecreaseDepth
	set_control(m_keys[1], 8, ""); // IncreaseDepth
	set_control(m_keys[1], 9, ""); // DecreaseConvergence
	set_control(m_keys[1], 10, ""); // IncreaseConvergence
	set_control(m_keys[1], 11, "`F1` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot1
	set_control(m_keys[1], 12, "`F2` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot2
	set_control(m_keys[1], 13, "`F3` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot3
	set_control(m_keys[1], 14, "`F4` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot4
	set_control(m_keys[1], 15, "`F5` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot5
	set_control(m_keys[1], 16, "`F6` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot6
	set_control(m_keys[1], 17, "`F7` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot7
	set_control(m_keys[1], 18, "`F8` & !(`Left Shift` | `Right Shift`)"); // LoadStateSlot8
	set_control(m_keys[1], 19, ""); // LoadStateSlot9
	set_control(m_keys[1], 20, ""); // LoadStateSlot10
	set_control(m_keys[1], 21, "(`Left Shift` | `Right Shift`) & `F1`"); // SaveStateSlot1
	set_control(m_keys[1], 22, "(`Left Shift` | `Right Shift`) & `F2`"); // SaveStateSlot2
	set_control(m_keys[1], 23, "(`Left Shift` | `Right Shift`) & `F3`"); // SaveStateSlot3
	set_control(m_keys[1], 24, "(`Left Shift` | `Right Shift`) & `F4`"); // SaveStateSlot4
	set_control(m_keys[1], 25, "(`Left Shift` | `Right Shift`) & `F5`"); // SaveStateSlot5
	set_control(m_keys[1], 26, "(`Left Shift` | `Right Shift`) & `F6`"); // SaveStateSlot6
	set_control(m_keys[1], 27, "(`Left Shift` | `Right Shift`) & `F7`"); // SaveStateSlot7
	set_control(m_keys[1], 28, "(`Left Shift` | `Right Shift`) & `F8`"); // SaveStateSlot8
	set_control(m_keys[1], 29, ""); // SaveStateSlot9
	set_control(m_keys[1], 30, ""); // SaveStateSlot10
	set_control(m_keys[1], 31, ""); // SelectStateSlot1
	set_control(m_keys[2], 0, ""); // SelectStateSlot2
	set_control(m_keys[2], 1, ""); // SelectStateSlot3
	set_control(m_keys[2], 2, ""); // SelectStateSlot4
	set_control(m_keys[2], 3, ""); // SelectStateSlot5
	set_control(m_keys[2], 4, ""); // SelectStateSlot6
	set_control(m_keys[2], 5, ""); // SelectStateSlot7
	set_control(m_keys[2], 6, ""); // SelectStateSlot8
	set_control(m_keys[2], 7, ""); // SelectStateSlot9
	set_control(m_keys[2], 8, ""); // SelectStateSlot10
	set_control(m_keys[2], 9, ""); // SaveSelectedSlot
	set_control(m_keys[2], 10, ""); // LoadSelectedSlot
	set_control(m_keys[2], 11, ""); // LoadLastState1
	set_control(m_keys[2], 12, ""); // LoadLastState2
	set_control(m_keys[2], 13, ""); // LoadLastState3
	set_control(m_keys[2], 14, ""); // LoadLastState4
	set_control(m_keys[2], 15, ""); // LoadLastState5
	set_control(m_keys[2], 16, ""); // LoadLastState6
	set_control(m_keys[2], 17, ""); // LoadLastState7
	set_control(m_keys[2], 18, ""); // LoadLastState8
	set_control(m_keys[2], 19, ""); // SaveFirstState
	set_control(m_keys[2], 20, "`F12` & !(`Left Shift` | `Right Shift`)"); // UndoLoadState
	set_control(m_keys[2], 21, "(`Left Shift` | `Right Shift`) & `F12`"); // UndoSaveState
	set_control(m_keys[2], 22, ""); // SaveStateFile
	set_control(m_keys[2], 23, ""); // LoadStateFile
#else // linux
	set_control(m_keys[0], 0, "(Control_L | Control_R) & `O`"); // Open
	set_control(m_keys[0], 1, ""); // ChangeDisc
	set_control(m_keys[0], 2, ""); // RefreshList
	set_control(m_keys[0], 3, "F10"); // PlayPause
	set_control(m_keys[0], 4, "Escape"); // Stop
	set_control(m_keys[0], 5, ""); // Reset
	set_control(m_keys[0], 6, ""); // FrameAdvance
	set_control(m_keys[0], 7, ""); // StartRecording
	set_control(m_keys[0], 8, ""); // PlayRecording
	set_control(m_keys[0], 9, ""); // ExportRecording
	set_control(m_keys[0], 10, ""); // Readonlymode
	set_control(m_keys[0], 11, "(Alt_L | Alt_R) & `Return`"); // ToggleFullscreen
	set_control(m_keys[0], 12, "`F9` & !(Alt_L | Alt_R)"); // Screenshot
	set_control(m_keys[0], 13, ""); // Exit
	set_control(m_keys[0], 14, "(Alt_L | Alt_R) & `F5`"); // Wiimote1Connect
	set_control(m_keys[0], 15, "(Alt_L | Alt_R) & `F6`"); // Wiimote2Connect
	set_control(m_keys[0], 16, "(Alt_L | Alt_R) & `F7`"); // Wiimote3Connect
	set_control(m_keys[0], 17, "(Alt_L | Alt_R) & `F8`"); // Wiimote4Connect
	set_control(m_keys[0], 18, "(Alt_L | Alt_R) & `F9`"); // BalanceBoardConnect
	set_control(m_keys[0], 19, ""); // VolumeDown
	set_control(m_keys[0], 20, ""); // VolumeUp
	set_control(m_keys[0], 21, ""); // VolumeToggleMute
	set_control(m_keys[0], 22, ""); // ToggleIR
	set_control(m_keys[0], 23, ""); // ToggleAspectRatio
	set_control(m_keys[0], 24, ""); // ToggleEFBCopies
	set_control(m_keys[0], 25, ""); // ToggleFog
	set_control(m_keys[0], 26, "Tab"); // ToggleThrottle
	set_control(m_keys[0], 27, ""); // DecreaseFrameLimit
	set_control(m_keys[0], 28, ""); // IncreaseFrameLimit
	set_control(m_keys[0], 29, "1"); // FreelookDecreaseSpeed
	set_control(m_keys[0], 30, "2"); // FreelookIncreaseSpeed
	set_control(m_keys[0], 31, "F"); // FreelookResetSpeed
	set_control(m_keys[1], 0, "E"); // FreelookUp
	set_control(m_keys[1], 1, "Q"); // FreelookDown
	set_control(m_keys[1], 2, "A"); // FreelookLeft
	set_control(m_keys[1], 3, "D"); // FreelookRight
	set_control(m_keys[1], 4, "W"); // FreelookZoomIn
	set_control(m_keys[1], 5, "S"); // FreelookZoomOut
	set_control(m_keys[1], 6, "R"); // FreelookReset
	set_control(m_keys[1], 7, ""); // DecreaseDepth
	set_control(m_keys[1], 8, ""); // IncreaseDepth
	set_control(m_keys[1], 9, ""); // DecreaseConvergence
	set_control(m_keys[1], 10, ""); // IncreaseConvergence
	set_control(m_keys[1], 11, "`F1` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot1
	set_control(m_keys[1], 12, "`F2` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot2
	set_control(m_keys[1], 13, "`F3` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot3
	set_control(m_keys[1], 14, "`F4` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot4
	set_control(m_keys[1], 15, "`F5` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot5
	set_control(m_keys[1], 16, "`F6` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot6
	set_control(m_keys[1], 17, "`F7` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot7
	set_control(m_keys[1], 18, "`F8` & !(Shift_L | Shift_R) & !(Alt_L | Alt_R)"); // LoadStateSlot8
	set_control(m_keys[1], 19, ""); // LoadStateSlot9
	set_control(m_keys[1], 20, ""); // LoadStateSlot10
	set_control(m_keys[1], 21, "(Shift_L | Shift_R) & `F1`"); // SaveStateSlot1
	set_control(m_keys[1], 22, "(Shift_L | Shift_R) & `F2`"); // SaveStateSlot2
	set_control(m_keys[1], 23, "(Shift_L | Shift_R) & `F3`"); // SaveStateSlot3
	set_control(m_keys[1], 24, "(Shift_L | Shift_R) & `F4`"); // SaveStateSlot4
	set_control(m_keys[1], 25, "(Shift_L | Shift_R) & `F5`"); // SaveStateSlot5
	set_control(m_keys[1], 26, "(Shift_L | Shift_R) & `F6`"); // SaveStateSlot6
	set_control(m_keys[1], 27, "(Shift_L | Shift_R) & `F7`"); // SaveStateSlot7
	set_control(m_keys[1], 28, "(Shift_L | Shift_R) & `F8`"); // SaveStateSlot8
	set_control(m_keys[1], 29, ""); // SaveStateSlot9
	set_control(m_keys[1], 30, ""); // SaveStateSlot10
	set_control(m_keys[1], 31, ""); // SelectStateSlot1
	set_control(m_keys[2], 0, ""); // SelectStateSlot2
	set_control(m_keys[2], 1, ""); // SelectStateSlot3
	set_control(m_keys[2], 2, ""); // SelectStateSlot4
	set_control(m_keys[2], 3, ""); // SelectStateSlot5
	set_control(m_keys[2], 4, ""); // SelectStateSlot6
	set_control(m_keys[2], 5, ""); // SelectStateSlot7
	set_control(m_keys[2], 6, ""); // SelectStateSlot8
	set_control(m_keys[2], 7, ""); // SelectStateSlot9
	set_control(m_keys[2], 8, ""); // SelectStateSlot10
	set_control(m_keys[2], 9, ""); // SaveSelectedSlot
	set_control(m_keys[2], 10, ""); // LoadSelectedSlot
	set_control(m_keys[2], 11, ""); // LoadLastState1
	set_control(m_keys[2], 12, ""); // LoadLastState2
	set_control(m_keys[2], 13, ""); // LoadLastState3
	set_control(m_keys[2], 14, ""); // LoadLastState4
	set_control(m_keys[2], 15, ""); // LoadLastState5
	set_control(m_keys[2], 16, ""); // LoadLastState6
	set_control(m_keys[2], 17, ""); // LoadLastState7
	set_control(m_keys[2], 18, ""); // LoadLastState8
	set_control(m_keys[2], 19, ""); // SaveFirstState
	set_control(m_keys[2], 20, "`F12` & !(Shift_L | Shift_R)"); // UndoLoadState
	set_control(m_keys[2], 21, "(Shift_L | Shift_R) & `F12`"); // UndoSaveState
	set_control(m_keys[2], 22, ""); // SaveStateFile
	set_control(m_keys[2], 23, ""); // LoadStateFile
#endif
}
