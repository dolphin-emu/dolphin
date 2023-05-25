// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HotkeyManager.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCPadStatus.h"

// clang-format off
constexpr std::array<const char*, NUM_HOTKEYS> s_hotkey_labels{{
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
    _trans("Unlock Cursor"),
    _trans("Center Mouse"),
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

    _trans("Next Profile"),
    _trans("Previous Profile"),
    _trans("Next Game Profile"),
    _trans("Previous Game Profile"),
    _trans("Next Profile"),
    _trans("Previous Profile"),
    _trans("Next Game Profile"),
    _trans("Previous Game Profile"),
    _trans("Next Profile"),
    _trans("Previous Profile"),
    _trans("Next Game Profile"),
    _trans("Previous Game Profile"),
    _trans("Next Profile"),
    _trans("Previous Profile"),
    _trans("Next Game Profile"),
    _trans("Previous Game Profile"),

    _trans("Toggle Crop"),
    _trans("Toggle Aspect Ratio"),
    _trans("Toggle Skip EFB Access"),
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

    _trans("Freelook Toggle"),

    _trans("Toggle 3D Side-by-Side"),
    _trans("Toggle 3D Top-Bottom"),
    _trans("Toggle 3D Anaglyph"),
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
    _trans("Increase Selected State Slot"),
    _trans("Decrease Selected State Slot"),

    _trans("Load ROM"),
    _trans("Unload ROM"),
    _trans("Reset"),

    _trans("Volume Down"),
    _trans("Volume Up"),
    _trans("Volume Toggle Mute"),
      
    _trans("1x"),
    _trans("2x"),
    _trans("3x"),
    _trans("4x"),

    _trans("Show Skylanders Portal"),
    _trans("Show Infinity Base")
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

void GetStatus(bool ignore_focus)
{
  // Get input
  static_cast<HotkeyManager*>(s_config.GetController(0))->GetInput(&s_hotkey, ignore_focus);
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

// This function exists to load the old "Keys" group so pre-existing configs don't break.
// TODO: Remove this at a future date when we're confident most configs are migrated.
static void LoadLegacyConfig(ControllerEmu::EmulatedController* controller)
{
  Common::IniFile inifile;
  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Hotkeys.ini"))
  {
    if (!inifile.Exists("Hotkeys") && inifile.Exists("Hotkeys1"))
    {
      auto sec = inifile.GetOrCreateSection("Hotkeys1");

      {
        std::string defdev;
        sec->Get("Device", &defdev, "");
        controller->SetDefaultDevice(defdev);
      }

      for (auto& group : controller->groups)
      {
        for (auto& control : group->controls)
        {
          std::string key("Keys/" + control->name);

          if (sec->Exists(key))
          {
            std::string expression;
            sec->Get(key, &expression, "");
            control->control_ref->SetExpression(std::move(expression));
          }
        }
      }

      controller->UpdateReferences(g_controller_interface);
    }
  }
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
    s_config.CreateController<HotkeyManager>();

  s_config.RegisterHotplugCallback();

  // load the saved controller config
  LoadConfig();

  s_hotkey_down = {};

  s_enabled = true;
}

void LoadConfig()
{
  s_config.LoadConfig(InputConfig::InputClass::GC);
  LoadLegacyConfig(s_config.GetController(0));
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
  bool ignore_focus = false;
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
     {_trans("Controller Profile 1"), HK_NEXT_WIIMOTE_PROFILE_1, HK_PREV_GAME_WIIMOTE_PROFILE_1},
     {_trans("Controller Profile 2"), HK_NEXT_WIIMOTE_PROFILE_2, HK_PREV_GAME_WIIMOTE_PROFILE_2},
     {_trans("Controller Profile 3"), HK_NEXT_WIIMOTE_PROFILE_3, HK_PREV_GAME_WIIMOTE_PROFILE_3},
     {_trans("Controller Profile 4"), HK_NEXT_WIIMOTE_PROFILE_4, HK_PREV_GAME_WIIMOTE_PROFILE_4},
     {_trans("Graphics Toggles"), HK_TOGGLE_CROP, HK_TOGGLE_TEXTURES},
     {_trans("Internal Resolution"), HK_INCREASE_IR, HK_DECREASE_IR},
     {_trans("Freelook"), HK_FREELOOK_TOGGLE, HK_FREELOOK_TOGGLE},
     // i18n: Stereoscopic 3D
     {_trans("3D"), HK_TOGGLE_STEREO_SBS, HK_TOGGLE_STEREO_ANAGLYPH},
     // i18n: Stereoscopic 3D
     {_trans("3D Depth"), HK_DECREASE_DEPTH, HK_INCREASE_CONVERGENCE},
     {_trans("Load State"), HK_LOAD_STATE_SLOT_1, HK_LOAD_STATE_SLOT_SELECTED},
     {_trans("Save State"), HK_SAVE_STATE_SLOT_1, HK_SAVE_STATE_SLOT_SELECTED},
     {_trans("Select State"), HK_SELECT_STATE_SLOT_1, HK_SELECT_STATE_SLOT_10},
     {_trans("Load Last State"), HK_LOAD_LAST_STATE_1, HK_LOAD_LAST_STATE_10},
     {_trans("Other State Hotkeys"), HK_SAVE_FIRST_STATE, HK_DECREMENT_SELECTED_STATE_SLOT},
     {_trans("GBA Core"), HK_GBA_LOAD, HK_GBA_RESET, true},
     {_trans("GBA Volume"), HK_GBA_VOLUME_DOWN, HK_GBA_TOGGLE_MUTE, true},
     {_trans("GBA Window Size"), HK_GBA_1X, HK_GBA_4X, true},
     {_trans("USB Emulation Devices"), HK_SKYLANDERS_PORTAL, HK_INFINITY_BASE}}};

HotkeyManager::HotkeyManager()
{
  for (std::size_t group = 0; group < m_hotkey_groups.size(); group++)
  {
    m_hotkey_groups[group] =
        (m_keys[group] = new ControllerEmu::Buttons(s_groups_info[group].name));
    groups.emplace_back(m_hotkey_groups[group]);
    for (int key = s_groups_info[group].first; key <= s_groups_info[group].last; key++)
    {
      m_keys[group]->AddInput(ControllerEmu::Translate, s_hotkey_labels[key]);
    }
  }
}

HotkeyManager::~HotkeyManager()
{
}

std::string HotkeyManager::GetName() const
{
  return "Hotkeys";
}

void HotkeyManager::GetInput(HotkeyStatus* kb, bool ignore_focus)
{
  const auto lock = GetStateLock();
  for (std::size_t group = 0; group < s_groups_info.size(); group++)
  {
    if (s_groups_info[group].ignore_focus != ignore_focus)
      continue;

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

  auto set_key_expression = [this](int index, const std::string& expression) {
    m_keys[FindGroupByID(index)]
        ->controls[GetIndexForGroup(FindGroupByID(index), index)]
        ->control_ref->SetExpression(expression);
  };

  auto hotkey_string = [](std::vector<std::string> inputs) {
    return "@(" + JoinStrings(inputs, "+") + ')';
  };

  // General hotkeys
  set_key_expression(HK_OPEN, hotkey_string({"Ctrl", "O"}));
  set_key_expression(HK_PLAY_PAUSE, "F10");
#ifdef _WIN32
  set_key_expression(HK_STOP, "ESCAPE");
  set_key_expression(HK_FULLSCREEN, hotkey_string({"Alt", "RETURN"}));
#else
  set_key_expression(HK_STOP, "Escape");
  set_key_expression(HK_FULLSCREEN, hotkey_string({"Alt", "Return"}));
#endif
  set_key_expression(HK_STEP, "F11");
  set_key_expression(HK_STEP_OVER, hotkey_string({"Shift", "F10"}));
  set_key_expression(HK_STEP_OUT, hotkey_string({"Shift", "F11"}));
  set_key_expression(HK_BP_TOGGLE, hotkey_string({"Shift", "F9"}));
  set_key_expression(HK_SCREENSHOT, "F9");
  set_key_expression(HK_WIIMOTE1_CONNECT, hotkey_string({"Alt", "F5"}));
  set_key_expression(HK_WIIMOTE2_CONNECT, hotkey_string({"Alt", "F6"}));
  set_key_expression(HK_WIIMOTE3_CONNECT, hotkey_string({"Alt", "F7"}));
  set_key_expression(HK_WIIMOTE4_CONNECT, hotkey_string({"Alt", "F8"}));
  set_key_expression(HK_BALANCEBOARD_CONNECT, hotkey_string({"Alt", "F9"}));
#ifdef _WIN32
  set_key_expression(HK_TOGGLE_THROTTLE, "TAB");
#else
  set_key_expression(HK_TOGGLE_THROTTLE, "Tab");
#endif

  // Savestates
  for (int i = 0; i < 8; i++)
  {
    set_key_expression(HK_LOAD_STATE_SLOT_1 + i, fmt::format("F{}", i + 1));
    set_key_expression(HK_SAVE_STATE_SLOT_1 + i,
                       hotkey_string({"Shift", fmt::format("F{}", i + 1)}));
  }
  set_key_expression(HK_UNDO_LOAD_STATE, "F12");
  set_key_expression(HK_UNDO_SAVE_STATE, hotkey_string({"Shift", "F12"}));

  // GBA
  set_key_expression(HK_GBA_LOAD, hotkey_string({"`Ctrl`", "`Shift`", "`O`"}));
  set_key_expression(HK_GBA_UNLOAD, hotkey_string({"`Ctrl`", "`Shift`", "`W`"}));
  set_key_expression(HK_GBA_RESET, hotkey_string({"`Ctrl`", "`Shift`", "`R`"}));

#ifdef _WIN32
  set_key_expression(HK_GBA_VOLUME_DOWN, "`SUBTRACT`");
  set_key_expression(HK_GBA_VOLUME_UP, "`ADD`");
#else
  set_key_expression(HK_GBA_VOLUME_DOWN, "`KP_Subtract`");
  set_key_expression(HK_GBA_VOLUME_UP, "`KP_Add`");
#endif
  set_key_expression(HK_GBA_TOGGLE_MUTE, "`M`");

#ifdef _WIN32
  set_key_expression(HK_GBA_1X, "`NUMPAD1`");
  set_key_expression(HK_GBA_2X, "`NUMPAD2`");
  set_key_expression(HK_GBA_3X, "`NUMPAD3`");
  set_key_expression(HK_GBA_4X, "`NUMPAD4`");
#else
  set_key_expression(HK_GBA_1X, "`KP_1`");
  set_key_expression(HK_GBA_2X, "`KP_2`");
  set_key_expression(HK_GBA_3X, "`KP_3`");
  set_key_expression(HK_GBA_4X, "`KP_4`");
#endif

  set_key_expression(HK_SKYLANDERS_PORTAL, hotkey_string({"Ctrl", "P"}));
  set_key_expression(HK_INFINITY_BASE, hotkey_string({"Ctrl", "I"}));
}
