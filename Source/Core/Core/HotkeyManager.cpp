// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HotkeyManager.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCPadStatus.h"

// clang-format off
constexpr std::array<const char*, 134> s_hotkey_labels{{
    _trans("Open"),
    _trans("Change Disc"),
    _trans("Eject Disc"),
    _trans("Refresh Game List"),
    _trans("Toggle Pause"),
    _trans("Stop"),
    _trans("Reset"),
    _trans("Toggle Fullscreen"),
    _trans("Take Screenshot"),
    _trans("Exit"),
    _trans("Activate NetPlay Chat"),
    _trans("Control NetPlay Golf Mode"),

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
    _trans("Read-Only Mode"),

    // i18n: Here, "Step" is a verb. This feature is used for
    // going through code step by step.
    _trans("Step Into"),
    // i18n: Here, "Step" is a verb. This feature is used for
    // going through code step by step.
    _trans("Step Over"),
    // i18n: Here, "Step" is a verb. This feature is used for
    // going through code step by step.
    _trans("Step Out"),
    _trans("Skip"),

    // i18n: Here, PC is an acronym for program counter, not personal computer.
    _trans("Show PC"),
    // i18n: Here, PC is an acronym for program counter, not personal computer.
    _trans("Set PC"),

    _trans("Toggle Breakpoint"),
    _trans("Add a Breakpoint"),
    _trans("Add a Memory Breakpoint"),

    _trans("Press Sync Button"),
    _trans("Connect Wii Remote 1"),
    _trans("Connect Wii Remote 2"),
    _trans("Connect Wii Remote 3"),
    _trans("Connect Wii Remote 4"),
    _trans("Connect Balance Board"),
    _trans("Toggle SD Card"),
    _trans("Toggle USB Keyboard"),

    _trans("Next Profile for Wii Remote 1"),
    _trans("Previous Profile for Wii Remote 1"),
    _trans("Next Game Profile for Wii Remote 1"),
    _trans("Previous Game Profile for Wii Remote 1"),
    _trans("Next Profile for Wii Remote 2"),
    _trans("Previous Profile for Wii Remote 2"),
    _trans("Next Game Profile for Wii Remote 2"),
    _trans("Previous Game Profile for Wii Remote 2"),
    _trans("Next Profile for Wii Remote 3"),
    _trans("Previous Profile for Wii Remote 3"),
    _trans("Next Game Profile for Wii Remote 3"),
    _trans("Previous Game Profile for Wii Remote 3"),
    _trans("Next Profile for Wii Remote 4"),
    _trans("Previous Profile for Wii Remote 4"),
    _trans("Next Game Profile for Wii Remote 4"),
    _trans("Previous Game Profile for Wii Remote 4"),

    _trans("Toggle Crop"),
    _trans("Toggle Aspect Ratio"),
    _trans("Toggle EFB Copies"),
    _trans("Toggle XFB Copies"),
    _trans("Toggle XFB Immediate Mode"),
    _trans("Toggle Fog"),
    _trans("Toggle Texture Dumping"),
    _trans("Toggle Custom Textures"),

    // i18n: IR stands for internal resolution
    _trans("Increase IR"),
    // i18n: IR stands for internal resolution
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

    _trans("Toggle 3D Side-by-Side"),
    _trans("Toggle 3D Top-Bottom"),
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
    _trans("Load from Selected Slot"),

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
    _trans("Save to Selected Slot"),

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
}};
// clang-format on
static_assert(NUM_HOTKEYS == s_hotkey_labels.size(), "Wrong count of hotkey_labels");

namespace HotkeyManagerEmu
{
static std::array<u32, NUM_HOTKEY_GROUPS> s_hotkey_down;
static HotkeyStatus s_hotkey;
static bool s_enabled;

static InputConfig s_config("Hotkeys", _trans("Hotkeys"), "Hotkeys");

InputConfig* GetConfig()
{
  return &s_config;
}

void GetStatus()
{
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
    const bool pressed = !!(s_hotkey_down[group] & (1 << group_key));
    s_hotkey_down[group] |= (1 << group_key);
    if (!pressed || held)
      return true;
  }
  else
  {
    s_hotkey_down[group] &= ~(1 << group_key);
  }

  return false;
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
    s_config.CreateController<HotkeyManager>();

  s_config.RegisterHotplugCallback();

  // load the saved controller config
  s_config.LoadConfig(true);

  s_hotkey_down = {};

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

void Shutdown()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();
}
}  // namespace HotkeyManagerEmu

struct HotkeyGroupInfo
{
  const char* name;
  Hotkey first;
  Hotkey last;
};

constexpr std::array<HotkeyGroupInfo, NUM_HOTKEY_GROUPS> s_groups_info = {
    {{_trans("General"), HK_OPEN, HK_REQUEST_GOLF_CONTROL},
     {_trans("Volume"), HK_VOLUME_DOWN, HK_VOLUME_TOGGLE_MUTE},
     {_trans("Emulation Speed"), HK_DECREASE_EMULATION_SPEED, HK_TOGGLE_THROTTLE},
     {_trans("Frame Advance"), HK_FRAME_ADVANCE, HK_FRAME_ADVANCE_RESET_SPEED},
     {_trans("Movie"), HK_START_RECORDING, HK_READ_ONLY_MODE},
     {_trans("Stepping"), HK_STEP, HK_SKIP},
     {_trans("Program Counter"), HK_SHOW_PC, HK_SET_PC},
     {_trans("Breakpoint"), HK_BP_TOGGLE, HK_MBP_ADD},
     {_trans("Wii"), HK_TRIGGER_SYNC_BUTTON, HK_TOGGLE_USB_KEYBOARD},
     {_trans("Controller Profile"), HK_NEXT_WIIMOTE_PROFILE_1, HK_PREV_GAME_WIIMOTE_PROFILE_4},
     {_trans("Graphics Toggles"), HK_TOGGLE_CROP, HK_TOGGLE_TEXTURES},
     {_trans("Internal Resolution"), HK_INCREASE_IR, HK_DECREASE_IR},
     {_trans("Freelook"), HK_FREELOOK_DECREASE_SPEED, HK_FREELOOK_RESET},
     // i18n: Stereoscopic 3D
     {_trans("3D"), HK_TOGGLE_STEREO_SBS, HK_TOGGLE_STEREO_3DVISION},
     // i18n: Stereoscopic 3D
     {_trans("3D Depth"), HK_DECREASE_DEPTH, HK_INCREASE_CONVERGENCE},
     {_trans("Load State"), HK_LOAD_STATE_SLOT_1, HK_LOAD_STATE_SLOT_SELECTED},
     {_trans("Save State"), HK_SAVE_STATE_SLOT_1, HK_SAVE_STATE_SLOT_SELECTED},
     {_trans("Select State"), HK_SELECT_STATE_SLOT_1, HK_SELECT_STATE_SLOT_10},
     {_trans("Load Last State"), HK_LOAD_LAST_STATE_1, HK_LOAD_LAST_STATE_10},
     {_trans("Other State Hotkeys"), HK_SAVE_FIRST_STATE, HK_LOAD_STATE_FILE}}};

HotkeyManager::HotkeyManager()
{
  for (std::size_t group = 0; group < m_hotkey_groups.size(); group++)
  {
    m_hotkey_groups[group] =
        (m_keys[group] = new ControllerEmu::Buttons("Keys", s_groups_info[group].name));
    groups.emplace_back(m_hotkey_groups[group]);
    for (int key = s_groups_info[group].first; key <= s_groups_info[group].last; key++)
    {
      m_keys[group]->controls.emplace_back(
          new ControllerEmu::Input(ControllerEmu::Translate, s_hotkey_labels[key]));
    }
  }
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
  const auto lock = GetStateLock();
  for (std::size_t group = 0; group < s_groups_info.size(); group++)
  {
    const int group_count = (s_groups_info[group].last - s_groups_info[group].first) + 1;
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

int HotkeyManager::FindGroupByID(int id) const
{
  const auto i = std::find_if(s_groups_info.begin(), s_groups_info.end(),
                              [id](const auto& entry) { return entry.last >= id; });

  return static_cast<int>(std::distance(s_groups_info.begin(), i));
}

int HotkeyManager::GetIndexForGroup(int group, int id) const
{
  return id - s_groups_info[group].first;
}

void HotkeyManager::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

#ifdef _WIN32
  const std::string NON = "(!(LMENU | RMENU) & !(LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
  const std::string ALT = "((LMENU | RMENU) & !(LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
  const std::string SHIFT = "(!(LMENU | RMENU) & (LSHIFT | RSHIFT) & !(LCONTROL | RCONTROL))";
  const std::string CTRL = "(!(LMENU | RMENU) & !(LSHIFT | RSHIFT) & (LCONTROL | RCONTROL))";
#elif __APPLE__
  const std::string NON =
      "(!`Left Alt` & !(`Left Shift`| `Right Shift`) & !(`Left Control` | `Right Control`))";
  const std::string ALT =
      "(`Left Alt` & !(`Left Shift`| `Right Shift`) & !(`Left Control` | `Right Control`))";
  const std::string SHIFT =
      "(!`Left Alt` & (`Left Shift`| `Right Shift`) & !(`Left Control` | `Right Control`))";
  const std::string CTRL =
      "(!`Left Alt` & !(`Left Shift`| `Right Shift`) & (`Left Control` | `Right Control`))";
#else
  const std::string NON = "(!`Alt_L` & !(`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
  const std::string ALT = "(`Alt_L` & !(`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
  const std::string SHIFT = "(!`Alt_L` & (`Shift_L` | `Shift_R`) & !(`Control_L` | `Control_R` ))";
  const std::string CTRL = "(!`Alt_L` & !(`Shift_L` | `Shift_R`) & (`Control_L` | `Control_R` ))";
#endif

  auto set_key_expression = [this](int index, const std::string& expression) {
    m_keys[FindGroupByID(index)]
        ->controls[GetIndexForGroup(FindGroupByID(index), index)]
        ->control_ref->SetExpression(expression);
  };

  // General hotkeys
  set_key_expression(HK_OPEN, CTRL + " & O");
  set_key_expression(HK_PLAY_PAUSE, NON + " & `F10`");
#ifdef _WIN32
  set_key_expression(HK_STOP, NON + " & ESCAPE");
  set_key_expression(HK_FULLSCREEN, ALT + " & RETURN");
#else
  set_key_expression(HK_STOP, NON + " & Escape");
  set_key_expression(HK_FULLSCREEN, ALT + " & Return");
#endif
  set_key_expression(HK_STEP, NON + " & `F11`");
  set_key_expression(HK_STEP_OVER, SHIFT + " & `F10`");
  set_key_expression(HK_STEP_OUT, SHIFT + " & `F11`");
  set_key_expression(HK_BP_TOGGLE, SHIFT + " & `F9`");
  set_key_expression(HK_SCREENSHOT, NON + " & `F9`");
  set_key_expression(HK_WIIMOTE1_CONNECT, ALT + " & `F5`");
  set_key_expression(HK_WIIMOTE2_CONNECT, ALT + " & `F6`");
  set_key_expression(HK_WIIMOTE3_CONNECT, ALT + " & `F7`");
  set_key_expression(HK_WIIMOTE4_CONNECT, ALT + " & `F8`");
  set_key_expression(HK_BALANCEBOARD_CONNECT, ALT + " & `F9`");
#ifdef _WIN32
  set_key_expression(HK_TOGGLE_THROTTLE, NON + " & TAB");
#else
  set_key_expression(HK_TOGGLE_THROTTLE, NON + " & Tab");
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
