// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <vector>

#include "Common/Common.h"
#include "Core/HotkeyManager.h"
#include "InputCommon/GCPadStatus.h"

const std::string hotkey_labels[] = {
    _trans("Open"),
    _trans("Change Disc"),
    _trans("Refresh List"),
    _trans("Toggle Pause"),
    _trans("Stop"),
    _trans("Reset"),
    _trans("Toggle Fullscreen"),
    _trans("Take Screenshot"),
    _trans("Exit"),

    _trans("Volume Down"),
    _trans("Volume Up"),
    _trans("Volume Toggle Mute"),

    _trans("Decrease Emulation Speed"),
    _trans("Increase Emulation Speed"),
    _trans("Disable Emulation Speed Limit"),

    _trans("Frame Advance"),
    _trans("Frame Advance Decrease Speed"),
    _trans("Frame Advance Increase Speed"),
    _trans("Frame Advance Reset Speed"),

    _trans("Start Recording"),
    _trans("Play Recording"),
    _trans("Export Recording"),
    _trans("Read-only mode"),

    _trans("Press Sync Button"),
    _trans("Connect Wii Remote 1"),
    _trans("Connect Wii Remote 2"),
    _trans("Connect Wii Remote 3"),
    _trans("Connect Wii Remote 4"),
    _trans("Connect Balance Board"),

    _trans("Toggle Crop"),
    _trans("Toggle Aspect Ratio"),
    _trans("Toggle EFB Copies"),
    _trans("Toggle Fog"),
    _trans("Toggle Custom Textures"),

    _trans("Increase IR"),
    _trans("Decrease IR"),

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
static_assert(NUM_HOTKEYS == sizeof(hotkey_labels) / sizeof(hotkey_labels[0]),
              "Wrong count of hotkey_labels");

namespace HotkeyManagerEmu
{
static u32 s_hotkeyDown[NUM_HOTKEY_GROUPS];
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

bool IsPressed(int id, bool held)
{
  unsigned int group = static_cast<HotkeyManager*>(s_config.GetController(0))->FindGroupByID(id);
  unsigned int group_key =
      static_cast<HotkeyManager*>(s_config.GetController(0))->GetIndexForGroup(group, id);
  if (s_hotkey.button[group] & (1 << group_key))
  {
    bool pressed = !!(s_hotkeyDown[group] & (1 << group_key));
    s_hotkeyDown[group] |= (1 << group_key);
    if (!pressed || held)
      return true;
  }
  else
  {
    s_hotkeyDown[group] &= ~(1 << group_key);
  }

  return false;
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
    s_config.CreateController<HotkeyManager>();

  g_controller_interface.RegisterHotplugCallback(LoadConfig);

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

ControllerEmu::ControlGroup* GetHotkeyGroup(HotkeyGroup group)
{
  return static_cast<HotkeyManager*>(s_config.GetController(0))->GetHotkeyGroup(group);
}

ControllerEmu::ControlGroup* GetOptionsGroup()
{
  return static_cast<HotkeyManager*>(s_config.GetController(0))->GetOptionsGroup();
}

void Shutdown()
{
  s_config.ClearControllers();
}
}

const std::array<HotkeyGroupInfo, NUM_HOTKEY_GROUPS> groups_info = {
    {{"General", HK_OPEN, HK_EXIT},
     {"Volume", HK_VOLUME_DOWN, HK_VOLUME_TOGGLE_MUTE},
     {"Emulation speed", HK_DECREASE_EMULATION_SPEED, HK_TOGGLE_THROTTLE},
     {"Frame advance", HK_FRAME_ADVANCE, HK_FRAME_ADVANCE_RESET_SPEED},
     {"Movie", HK_START_RECORDING, HK_READ_ONLY_MODE},
     {"Wii", HK_TRIGGER_SYNC_BUTTON, HK_BALANCEBOARD_CONNECT},
     {"Graphics toggles", HK_TOGGLE_CROP, HK_TOGGLE_TEXTURES},
     {"Internal Resolution", HK_INCREASE_IR, HK_DECREASE_IR},
     {"Freelook", HK_FREELOOK_DECREASE_SPEED, HK_FREELOOK_RESET},
     {"3D", HK_TOGGLE_STEREO_SBS, HK_TOGGLE_STEREO_3DVISION},
     {"3D depth", HK_DECREASE_DEPTH, HK_INCREASE_CONVERGENCE},
     {"Load state", HK_LOAD_STATE_SLOT_1, HK_LOAD_STATE_SLOT_SELECTED},
     {"Save state", HK_SAVE_STATE_SLOT_1, HK_SAVE_STATE_SLOT_SELECTED},
     {"Select state", HK_SELECT_STATE_SLOT_1, HK_SELECT_STATE_SLOT_10},
     {"Load last state", HK_LOAD_LAST_STATE_1, HK_LOAD_LAST_STATE_10},
     {"Other state hotkeys", HK_SAVE_FIRST_STATE, HK_LOAD_STATE_FILE}}};

HotkeyManager::HotkeyManager()
{
  for (int group = 0; group < NUM_HOTKEY_GROUPS; group++)
  {
    m_hotkey_groups[group] = (m_keys[group] = new Buttons("Keys", _trans(groups_info[group].name)));
    groups.emplace_back(m_hotkey_groups[group]);
    for (int key = groups_info[group].first; key <= groups_info[group].last; key++)
    {
      m_keys[group]->controls.emplace_back(new ControlGroup::Input(hotkey_labels[key]));
    }
  }

  groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
  m_options->boolean_settings.emplace_back(
      std::make_unique<ControlGroup::BackgroundInputSetting>(_trans("Background Input")));
  m_options->boolean_settings.emplace_back(std::make_unique<ControlGroup::BooleanSetting>(
      _trans("Iterative Input"), false, ControlGroup::SettingType::VIRTUAL));
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
  auto lock = ControllerEmu::GetStateLock();
  for (int group = 0; group < NUM_HOTKEY_GROUPS; group++)
  {
    const int group_count = (groups_info[group].last - groups_info[group].first) + 1;
    std::vector<u32> bitmasks(group_count);
    for (size_t key = 0; key < bitmasks.size(); key++)
      bitmasks[key] = static_cast<u32>(1 << key);

    kb->button[group] = 0;
    m_keys[group]->GetState(&kb->button[group], bitmasks.data());
  }
}

ControllerEmu::ControlGroup* HotkeyManager::GetHotkeyGroup(HotkeyGroup group) const
{
  return m_hotkey_groups[group];
}

ControllerEmu::ControlGroup* HotkeyManager::GetOptionsGroup() const
{
  return m_options;
}

int HotkeyManager::FindGroupByID(int id) const
{
  const auto i = std::find_if(groups_info.begin(), groups_info.end(),
                              [id](const auto& entry) { return entry.last >= id; });

  return static_cast<int>(std::distance(groups_info.begin(), i));
}

int HotkeyManager::GetIndexForGroup(int group, int id) const
{
  return id - groups_info[group].first;
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
    m_keys[FindGroupByID(index)]
        ->controls[GetIndexForGroup(FindGroupByID(index), index)]
        ->control_ref->expression = expression;
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
    set_key_expression(HK_LOAD_STATE_SLOT_1 + i,
                       StringFromFormat((NON + " & `F%d`").c_str(), i + 1));
    set_key_expression(HK_SAVE_STATE_SLOT_1 + i,
                       StringFromFormat((SHIFT + " & `F%d`").c_str(), i + 1));
  }
  set_key_expression(HK_UNDO_LOAD_STATE, NON + " & `F12`");
  set_key_expression(HK_UNDO_SAVE_STATE, SHIFT + " & `F12`");
}
