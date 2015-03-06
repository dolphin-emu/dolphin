// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/HotkeyManager.h"

const std::string hotkey_labels[] =
{
	(""), // Open
	(""), // Change Disc
	(""), // Refresh List

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
	(""), // Exit

	_trans("Connect Wiimote 1"),
	_trans("Connect Wiimote 2"),
	_trans("Connect Wiimote 3"),
	_trans("Connect Wiimote 4"),
	_trans("Connect Balance Board"),

	_trans("Volume Down"),
	_trans("Volume Up"),
	_trans("Volume Toggle Mute"),

	_trans("Toggle IR"),
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

	_trans("Save Oldest State"),
	_trans("Undo Load State"),
	_trans("Undo Save State"),
	_trans("Save State"),
	_trans("Load State"),
};

const int num_hotkeys = (sizeof(hotkey_labels) / sizeof(hotkey_labels[0]));

namespace HotkeyManagerEmu
{

static u32 hotkeyDown[3];
static HotkeyStatus hotkey;

static InputConfig s_config("Hotkeys", _trans("Hotkeys"), "Hotkeys");

InputConfig* GetConfig()
{
	return &s_config;
}

void GetStatus()
{
	hotkey.err = PAD_ERR_NONE;

	// get input
	((HotkeyManager*)s_config.controllers[0])->GetInput(&hotkey);
}

bool IsEnabled()
{
	return enabled;
}

void Enable(bool enable_toggle)
{
	enabled = enable_toggle;
}

bool IsPressed(int Id, bool held)
{
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

	for (unsigned int i = 0; i < 3; ++i)
		hotkeyDown[i] = 0;

	enabled = true;
}

void LoadConfig()
{
	s_config.LoadConfig(true);
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
	for (int set = 0; set < 3; set++)
	{
		// buttons
		if ((set * 32) < num_hotkeys)
			groups.emplace_back(m_keys[set] = new Buttons(_trans("Keys")));

		for (int key = 0; key < 32; key++)
		{
			if ((set * 32 + key) < num_hotkeys && hotkey_labels[set * 32 + key].length() != 0)
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
	for (int set = 0; set < 3; set++)
	{
		std::vector<u32> bitmasks;
		for (int key = 0; key < 32; key++)
		{
			if (hotkey_labels[set * 32 + key].length() != 0)
				bitmasks.push_back(1 << key);
		}

		if ((set * 32) < num_hotkeys)
		{
			kb->button[set] = 0;
			m_keys[set]->GetState(&kb->button[set], bitmasks.data());
		}
	}
}

void HotkeyManager::LoadDefaults(const ControllerInterface& ciface)
{
#define set_control(group, num, str)  (group)->controls[num]->control_ref->expression = (str)

	ControllerEmu::LoadDefaults(ciface);

	// Buttons
	set_control(m_keys[0], 0, ""); // Open
	set_control(m_keys[0], 1, ""); // ChangeDisc
	set_control(m_keys[0], 2, ""); // RefreshList
	set_control(m_keys[0], 3, ""); // PlayPause
	set_control(m_keys[0], 4, ""); // Stop
	set_control(m_keys[0], 5, ""); // Reset
	set_control(m_keys[0], 6, ""); // FrameAdvance
	set_control(m_keys[0], 7, ""); // StartRecording
	set_control(m_keys[0], 8, ""); // PlayRecording
	set_control(m_keys[0], 9, ""); // ExportRecording
	set_control(m_keys[0], 10, ""); // Readonlymode
	set_control(m_keys[0], 11, ""); // ToggleFullscreen
	set_control(m_keys[0], 12, ""); // Screenshot
	set_control(m_keys[0], 13, ""); // Exit
	set_control(m_keys[0], 14, ""); // Wiimote1Connect
	set_control(m_keys[0], 15, ""); // Wiimote2Connect
	set_control(m_keys[0], 16, ""); // Wiimote3Connect
	set_control(m_keys[0], 17, ""); // Wiimote4Connect
	set_control(m_keys[0], 18, ""); // BalanceBoardConnect
	set_control(m_keys[0], 19, ""); // VolumeDown
	set_control(m_keys[0], 20, ""); // VolumeUp
	set_control(m_keys[0], 21, ""); // VolumeToggleMute
	set_control(m_keys[0], 22, ""); // ToggleIR
	set_control(m_keys[0], 23, ""); // ToggleAspectRatio
	set_control(m_keys[0], 24, ""); // ToggleEFBCopies
	set_control(m_keys[0], 25, ""); // ToggleFog
	set_control(m_keys[0], 26, ""); // ToggleThrottle
	set_control(m_keys[0], 27, ""); // DecreaseFrameLimit
	set_control(m_keys[0], 28, ""); // IncreaseFrameLimit
	set_control(m_keys[0], 29, ""); // FreelookDecreaseSpeed
	set_control(m_keys[0], 30, ""); // FreelookIncreaseSpeed
	set_control(m_keys[0], 31, ""); // FreelookResetSpeed
	set_control(m_keys[1], 0, ""); // FreelookUp
	set_control(m_keys[1], 1, ""); // FreelookDown
	set_control(m_keys[1], 2, ""); // FreelookLeft
	set_control(m_keys[1], 3, ""); // FreelookRight
	set_control(m_keys[1], 4, ""); // FreelookZoomIn
	set_control(m_keys[1], 5, ""); // FreelookZoomOut
	set_control(m_keys[1], 6, ""); // FreelookReset
	set_control(m_keys[1], 7, ""); // DecreaseDepth
	set_control(m_keys[1], 8, ""); // IncreaseDepth
	set_control(m_keys[1], 9, ""); // DecreaseConvergence
	set_control(m_keys[1], 10, ""); // IncreaseConvergence
	set_control(m_keys[1], 11, ""); // LoadStateSlot1
	set_control(m_keys[1], 12, ""); // LoadStateSlot2
	set_control(m_keys[1], 13, ""); // LoadStateSlot3
	set_control(m_keys[1], 14, ""); // LoadStateSlot4
	set_control(m_keys[1], 15, ""); // LoadStateSlot5
	set_control(m_keys[1], 16, ""); // LoadStateSlot6
	set_control(m_keys[1], 17, ""); // LoadStateSlot7
	set_control(m_keys[1], 18, ""); // LoadStateSlot8
	set_control(m_keys[1], 19, ""); // LoadStateSlot9
	set_control(m_keys[1], 20, ""); // LoadStateSlot10
	set_control(m_keys[1], 21, ""); // SaveStateSlot1
	set_control(m_keys[1], 22, ""); // SaveStateSlot2
	set_control(m_keys[1], 23, ""); // SaveStateSlot3
	set_control(m_keys[1], 24, ""); // SaveStateSlot4
	set_control(m_keys[1], 25, ""); // SaveStateSlot5
	set_control(m_keys[1], 26, ""); // SaveStateSlot6
	set_control(m_keys[1], 27, ""); // SaveStateSlot7
	set_control(m_keys[1], 28, ""); // SaveStateSlot8
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
	set_control(m_keys[2], 20, ""); // UndoLoadState
	set_control(m_keys[2], 21, ""); // UndoSaveState
	set_control(m_keys[2], 22, ""); // SaveStateFile
	set_control(m_keys[2], 23, ""); // LoadStateFile
}
