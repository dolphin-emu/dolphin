// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Movie.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iterator>
#include <locale>
#include <mbedtls/config.h>
#include <mbedtls/md.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IOFile.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Common/Version.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigLoaders/MovieConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSP/DSPCore.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/EXI/EXI_DeviceMemoryCard.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"

#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/NetPlayProto.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Enums.h"

#include "InputCommon/GCPadStatus.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

// The chunk to allocate movie data in multiples of.
#define DTM_BASE_LENGTH (1024)

namespace Movie
{
using namespace WiimoteCommon;
using namespace WiimoteEmu;

static bool IsMovieHeader(const std::array<u8, 4>& magic)
{
  return magic[0] == 'D' && magic[1] == 'T' && magic[2] == 'M' && magic[3] == 0x1A;
}

static std::array<u8, 20> ConvertGitRevisionToBytes(const std::string& revision)
{
  std::array<u8, 20> revision_bytes{};

  if (revision.size() % 2 == 0 && std::all_of(revision.begin(), revision.end(), ::isxdigit))
  {
    // The revision string normally contains a git commit hash,
    // which is 40 hexadecimal digits long. In DTM files, each pair of
    // hexadecimal digits is stored as one byte, for a total of 20 bytes.
    size_t bytes_to_write = std::min(revision.size() / 2, revision_bytes.size());
    unsigned int temp;
    for (size_t i = 0; i < bytes_to_write; ++i)
    {
      sscanf(&revision[2 * i], "%02x", &temp);
      revision_bytes[i] = temp;
    }
  }
  else
  {
    // If the revision string for some reason doesn't only contain hexadecimal digit
    // pairs, we instead copy the string with no conversion. This probably doesn't match
    // the intended design of the DTM format, but it's the most sensible fallback.
    size_t bytes_to_write = std::min(revision.size(), revision_bytes.size());
    std::copy_n(std::begin(revision), bytes_to_write, std::begin(revision_bytes));
  }

  return revision_bytes;
}

MovieManager::MovieManager(Core::System& system) : m_system(system)
{
}

MovieManager::~MovieManager() = default;

// NOTE: GPU Thread
std::string MovieManager::GetInputDisplay()
{
  if (!IsMovieActive())
  {
    m_controllers = {};
    m_wiimotes = {};

    const auto& si = m_system.GetSerialInterface();
    for (int i = 0; i < 4; ++i)
    {
      if (si.GetDeviceType(i) == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
        m_controllers[i] = ControllerType::GBA;
      else if (si.GetDeviceType(i) != SerialInterface::SIDEVICE_NONE)
        m_controllers[i] = ControllerType::GC;
      else
        m_controllers[i] = ControllerType::None;
      m_wiimotes[i] = Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::None;
    }
  }

  std::string input_display;
  {
    std::lock_guard guard(m_input_display_lock);
    for (int i = 0; i < 4; ++i)
    {
      if (IsUsingPad(i))
        input_display += m_input_display[i] + '\n';
    }
    for (int i = 0; i < 4; ++i)
    {
      if (IsUsingWiimote(i))
        input_display += m_input_display[i + 4] + '\n';
    }
  }
  return input_display;
}

// NOTE: GPU Thread
std::string MovieManager::GetRTCDisplay() const
{
  using ExpansionInterface::CEXIIPL;

  const time_t current_time = CEXIIPL::GetEmulatedTime(m_system, CEXIIPL::UNIX_EPOCH);
  const tm gm_time = fmt::gmtime(current_time);

  // Use current locale for formatting time, as fmt is locale-agnostic by default.
  return fmt::format(std::locale{""}, "Date/Time: {:%c}", gm_time);
}

// NOTE: GPU Thread
std::string MovieManager::GetRerecords() const
{
  if (IsMovieActive())
    return fmt::format("Rerecords: {}", m_rerecords);

  return "Rerecords: N/A";
}

void MovieManager::FrameUpdate()
{
  m_current_frame++;
  if (!m_polled)
    m_current_lag_count++;

  if (IsRecordingInput())
  {
    m_total_frames = m_current_frame;
    m_total_lag_count = m_current_lag_count;
  }

  m_polled = false;
}

// called when game is booting up, even if no movie is active,
// but potentially after BeginRecordingInput or PlayInput has been called.
// NOTE: EmuThread
void MovieManager::Init(const BootParameters& boot)
{
  if (std::holds_alternative<BootParameters::Disc>(boot.parameters))
    m_current_file_name = std::get<BootParameters::Disc>(boot.parameters).path;
  else
    m_current_file_name.clear();

  m_polled = false;
  m_save_config = false;
  if (IsPlayingInput())
  {
    ReadHeader();
    std::thread md5thread(&MovieManager::CheckMD5, this);
    md5thread.detach();
    if (strncmp(m_temp_header.gameID.data(), SConfig::GetInstance().GetGameID().c_str(), 6))
    {
      PanicAlertFmtT("The recorded game ({0}) is not the same as the selected game ({1})",
                     m_temp_header.GetGameID(), SConfig::GetInstance().GetGameID());
      EndPlayInput(false);
    }
  }

  if (IsRecordingInput())
  {
    GetSettings();
    std::thread md5thread(&MovieManager::GetMD5, this);
    md5thread.detach();
    m_tick_count_at_last_input = 0;
  }

  memset(&m_pad_state, 0, sizeof(m_pad_state));

  for (auto& disp : m_input_display)
    disp.clear();

  if (!IsMovieActive())
  {
    m_recording_from_save_state = false;
    m_rerecords = 0;
    m_current_byte = 0;
    m_current_frame = 0;
    m_current_lag_count = 0;
    m_current_input_count = 0;
  }
}

// NOTE: CPU Thread
void MovieManager::InputUpdate()
{
  m_current_input_count++;

  if (!IsRecordingInput())
    return;

  const auto& core_timing = m_system.GetCoreTiming();
  m_total_input_count = m_current_input_count;
  m_total_tick_count += core_timing.GetTicks() - m_tick_count_at_last_input;
  m_tick_count_at_last_input = core_timing.GetTicks();
}

// NOTE: CPU Thread
void MovieManager::SetPolledDevice()
{
  m_polled = true;
}

// NOTE: Host Thread
void MovieManager::SetReadOnly(bool bEnabled)
{
  if (m_read_only != bEnabled)
    Core::DisplayMessage(bEnabled ? "Read-only mode." : "Read+Write mode.", 1000);

  m_read_only = bEnabled;
}

bool MovieManager::IsRecordingInput() const
{
  return (m_play_mode == PlayMode::Recording);
}

bool MovieManager::IsRecordingInputFromSaveState() const
{
  return m_recording_from_save_state;
}

bool MovieManager::IsJustStartingRecordingInputFromSaveState() const
{
  return IsRecordingInputFromSaveState() && m_current_frame == 0;
}

bool MovieManager::IsJustStartingPlayingInputFromSaveState() const
{
  return IsRecordingInputFromSaveState() && m_current_frame == 1 && IsPlayingInput();
}

bool MovieManager::IsPlayingInput() const
{
  return (m_play_mode == PlayMode::Playing);
}

bool MovieManager::IsMovieActive() const
{
  return m_play_mode != PlayMode::None;
}

bool MovieManager::IsReadOnly() const
{
  return m_read_only;
}

u64 MovieManager::GetRecordingStartTime() const
{
  return m_recording_start_time;
}

u64 MovieManager::GetCurrentFrame() const
{
  return m_current_frame;
}

u64 MovieManager::GetTotalFrames() const
{
  return m_total_frames;
}

u64 MovieManager::GetCurrentInputCount() const
{
  return m_current_input_count;
}

u64 MovieManager::GetTotalInputCount() const
{
  return m_total_input_count;
}

u64 MovieManager::GetCurrentLagCount() const
{
  return m_current_lag_count;
}

u64 MovieManager::GetTotalLagCount() const
{
  return m_total_lag_count;
}

void MovieManager::SetClearSave(bool enabled)
{
  m_clear_save = enabled;
}

void MovieManager::SignalDiscChange(const std::string& new_path)
{
  if (IsRecordingInput())
  {
    size_t size_of_path_without_filename = new_path.find_last_of("/\\") + 1;
    std::string filename = new_path.substr(size_of_path_without_filename);
    constexpr size_t maximum_length = sizeof(DTMHeader::discChange);
    if (filename.length() > maximum_length)
    {
      PanicAlertFmtT("The disc change to \"{0}\" could not be saved in the .dtm file.\n"
                     "The filename of the disc image must not be longer than 40 characters.",
                     filename);
    }
    m_disc_change_filename = filename;
    m_has_disc_change = true;
  }
}

void MovieManager::SetReset(bool reset)
{
  m_reset = reset;
}

bool MovieManager::IsUsingPad(int controller) const
{
  return m_controllers[controller] != ControllerType::None;
}

bool MovieManager::IsUsingBongo(int controller) const
{
  return ((m_bongos & (1 << controller)) != 0);
}

bool MovieManager::IsUsingGBA(int controller) const
{
  return m_controllers[controller] == ControllerType::GBA;
}

bool MovieManager::IsUsingWiimote(int wiimote) const
{
  return m_wiimotes[wiimote];
}

bool MovieManager::IsConfigSaved() const
{
  return m_save_config;
}

bool MovieManager::IsStartingFromClearSave() const
{
  return m_clear_save;
}

bool MovieManager::IsUsingMemcard(ExpansionInterface::Slot slot) const
{
  switch (slot)
  {
  case ExpansionInterface::Slot::A:
    return (m_memcards & 1) != 0;
  case ExpansionInterface::Slot::B:
    return (m_memcards & 2) != 0;
  default:
    return false;
  }
}

bool MovieManager::IsNetPlayRecording() const
{
  return m_net_play;
}

// NOTE: Host Thread
void MovieManager::ChangePads()
{
  if (!Core::IsRunning(m_system))
    return;

  ControllerTypeArray controllers{};

  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
    if (si_device == SerialInterface::SIDEVICE_GC_GBA_EMULATED)
      controllers[i] = ControllerType::GBA;
    else if (SerialInterface::SIDevice_IsGCController(si_device))
      controllers[i] = ControllerType::GC;
    else
      controllers[i] = ControllerType::None;
  }

  if (m_controllers == controllers)
    return;

  auto& si = m_system.GetSerialInterface();
  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    SerialInterface::SIDevices device = SerialInterface::SIDEVICE_NONE;
    if (IsUsingGBA(i))
    {
      device = SerialInterface::SIDEVICE_GC_GBA_EMULATED;
    }
    else if (IsUsingPad(i))
    {
      const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
      if (SerialInterface::SIDevice_IsGCController(si_device))
      {
        device = si_device;
      }
      else
      {
        device = IsUsingBongo(i) ? SerialInterface::SIDEVICE_GC_TARUKONGA :
                                   SerialInterface::SIDEVICE_GC_CONTROLLER;
      }
    }

    si.ChangeDevice(device, i);
  }
}

// NOTE: Host / Emu Threads
void MovieManager::ChangeWiiPads(bool instantly)
{
  WiimoteEnabledArray wiimotes{};

  for (int i = 0; i < MAX_WIIMOTES; ++i)
  {
    wiimotes[i] = Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::None;
  }

  // This is important for Wiimotes, because they can desync easily if they get re-activated
  if (instantly && m_wiimotes == wiimotes)
    return;

  const auto bt = WiiUtils::GetBluetoothEmuDevice();
  for (int i = 0; i < MAX_WIIMOTES; ++i)
  {
    const bool is_using_wiimote = IsUsingWiimote(i);

    Config::SetCurrent(Config::GetInfoForWiimoteSource(i),
                       is_using_wiimote ? WiimoteSource::Emulated : WiimoteSource::None);
    if (bt != nullptr)
      bt->AccessWiimoteByIndex(i)->Activate(is_using_wiimote);
  }
}

// NOTE: Host Thread
bool MovieManager::BeginRecordingInput(const ControllerTypeArray& controllers,
                                       const WiimoteEnabledArray& wiimotes)
{
  if (m_play_mode != PlayMode::None ||
      (controllers == ControllerTypeArray{} && wiimotes == WiimoteEnabledArray{}))
    return false;

  const auto start_recording = [this, controllers, wiimotes] {
    m_controllers = controllers;
    m_wiimotes = wiimotes;
    m_current_frame = m_total_frames = 0;
    m_current_lag_count = m_total_lag_count = 0;
    m_current_input_count = m_total_input_count = 0;
    m_total_tick_count = m_tick_count_at_last_input = 0;
    m_bongos = 0;
    m_memcards = 0;
    if (NetPlay::IsNetPlayRunning())
    {
      m_net_play = true;
      m_recording_start_time = ExpansionInterface::CEXIIPL::NetPlay_GetEmulatedTime();
    }
    else if (Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE))
    {
      m_recording_start_time = Config::Get(Config::MAIN_CUSTOM_RTC_VALUE);
    }
    else
    {
      m_recording_start_time = Common::Timer::GetLocalTimeSinceJan1970();
    }

    m_rerecords = 0;

    for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
    {
      const SerialInterface::SIDevices si_device = Config::Get(Config::GetInfoForSIDevice(i));
      if (si_device == SerialInterface::SIDEVICE_GC_TARUKONGA)
        m_bongos |= (1 << i);
    }

    if (Core::IsRunning(m_system))
    {
      const std::string save_path = File::GetUserPath(D_STATESAVES_IDX) + "dtm.sav";
      if (File::Exists(save_path))
        File::Delete(save_path);

      State::SaveAs(m_system, save_path);
      m_recording_from_save_state = true;

      std::thread md5thread(&MovieManager::GetMD5, this);
      md5thread.detach();
      GetSettings();
    }

    // Wiimotes cause desync issues if they're not reset before launching the game
    if (!Core::IsRunning(m_system))
    {
      // This will also reset the Wiimotes for GameCube games, but that shouldn't do anything
      Wiimote::ResetAllWiimotes();
    }

    m_play_mode = PlayMode::Recording;
    m_author = Config::Get(Config::MAIN_MOVIE_MOVIE_AUTHOR);
    m_temp_input.clear();

    m_current_byte = 0;

    // This is a bit of a hack, SYSCONF movie code expects the movie layer active for both recording
    // and playback. That layer is really only designed for playback, not recording. Also, we can't
    // know if we're using a Wii at this point. So, we'll assume a Wii is used here. In practice,
    // this shouldn't affect anything for GC (as its only unique setting is language, which will be
    // taken from base settings as expected)
    static DTMHeader header = {.bWii = true};
    ConfigLoaders::SaveToDTM(&header);
    Config::AddLayer(ConfigLoaders::GenerateMovieConfigLoader(&header));

    if (Core::IsRunning(m_system))
      Core::UpdateWantDeterminism(m_system);
  };
  Core::RunOnCPUThread(m_system, start_recording, true);

  Core::DisplayMessage("Starting movie recording", 2000);
  return true;
}

static std::string Analog2DToString(u32 x, u32 y, const std::string& prefix, u32 range = 255)
{
  const u32 center = range / 2 + 1;

  if ((x <= 1 || x == center || x >= range) && (y <= 1 || y == center || y >= range))
  {
    if (x != center || y != center)
    {
      if (x != center && y != center)
      {
        return fmt::format("{}:{},{}", prefix, x < center ? "LEFT" : "RIGHT",
                           y < center ? "DOWN" : "UP");
      }

      if (x != center)
      {
        return fmt::format("{}:{}", prefix, x < center ? "LEFT" : "RIGHT");
      }

      return fmt::format("{}:{}", prefix, y < center ? "DOWN" : "UP");
    }

    return "";
  }

  return fmt::format("{}:{},{}", prefix, x, y);
}

static std::string Analog1DToString(u32 v, const std::string& prefix, u32 range = 255)
{
  if (v == 0)
    return "";

  if (v == range)
    return prefix;

  return fmt::format("{}:{}", prefix, v);
}

// NOTE: CPU Thread
static std::string GenerateInputDisplayString(ControllerState padState, int controllerID)
{
  std::string display_str = fmt::format("P{}:", controllerID + 1);

  if (padState.is_connected)
  {
    if (padState.A)
      display_str += " A";
    if (padState.B)
      display_str += " B";
    if (padState.X)
      display_str += " X";
    if (padState.Y)
      display_str += " Y";
    if (padState.Z)
      display_str += " Z";
    if (padState.Start)
      display_str += " START";

    if (padState.DPadUp)
      display_str += " UP";
    if (padState.DPadDown)
      display_str += " DOWN";
    if (padState.DPadLeft)
      display_str += " LEFT";
    if (padState.DPadRight)
      display_str += " RIGHT";
    if (padState.reset)
      display_str += " RESET";

    if (padState.TriggerL == 255 || padState.L)
      display_str += " L";
    else
      display_str += Analog1DToString(padState.TriggerL, " L");

    if (padState.TriggerR == 255 || padState.R)
      display_str += " R";
    else
      display_str += Analog1DToString(padState.TriggerR, " R");

    display_str += Analog2DToString(padState.AnalogStickX, padState.AnalogStickY, " ANA");
    display_str += Analog2DToString(padState.CStickX, padState.CStickY, " C");
  }
  else
  {
    display_str += " DISCONNECTED";
  }

  return display_str;
}

// NOTE: CPU Thread
static std::string GenerateWiiInputDisplayString(int remoteID, const DataReportBuilder& rpt,
                                                 ExtensionNumber ext, const EncryptionKey& key)
{
  std::string display_str = fmt::format("R{}:", remoteID + 1);

  if (rpt.HasCore())
  {
    ButtonData buttons;
    rpt.GetCoreData(&buttons);

    if (buttons.left)
      display_str += " LEFT";
    if (buttons.right)
      display_str += " RIGHT";
    if (buttons.down)
      display_str += " DOWN";
    if (buttons.up)
      display_str += " UP";
    if (buttons.a)
      display_str += " A";
    if (buttons.b)
      display_str += " B";
    if (buttons.plus)
      display_str += " +";
    if (buttons.minus)
      display_str += " -";
    if (buttons.one)
      display_str += " 1";
    if (buttons.two)
      display_str += " 2";
    if (buttons.home)
      display_str += " HOME";
  }

  if (rpt.HasAccel())
  {
    AccelData accel_data;
    rpt.GetAccelData(&accel_data);

    // FYI: This will only print partial data for interleaved reports.

    display_str +=
        fmt::format(" ACC:{},{},{}", accel_data.value.x, accel_data.value.y, accel_data.value.z);
  }

  if (rpt.HasIR())
  {
    const u8* const ir_data = rpt.GetIRDataPtr();

    // TODO: This does not handle the different IR formats.

    const u16 x = ir_data[0] | ((ir_data[2] >> 4 & 0x3) << 8);
    const u16 y = ir_data[1] | ((ir_data[2] >> 6 & 0x3) << 8);
    display_str += fmt::format(" IR:{},{}", x, y);
  }

  // Nunchuk
  if (rpt.HasExt() && ext == ExtensionNumber::NUNCHUK)
  {
    const u8* const extData = rpt.GetExtDataPtr();

    Nunchuk::DataFormat nunchuk;
    memcpy(&nunchuk, extData, sizeof(nunchuk));
    key.Decrypt((u8*)&nunchuk, 0, sizeof(nunchuk));
    nunchuk.bt.hex = nunchuk.bt.hex ^ 0x3;

    const std::string accel = fmt::format(" N-ACC:{},{},{}", nunchuk.GetAccelX(),
                                          nunchuk.GetAccelY(), nunchuk.GetAccelZ());

    if (nunchuk.bt.c)
      display_str += " C";
    if (nunchuk.bt.z)
      display_str += " Z";
    display_str += accel;
    display_str += Analog2DToString(nunchuk.jx, nunchuk.jy, " ANA");
  }

  // Classic controller
  if (rpt.HasExt() && ext == ExtensionNumber::CLASSIC)
  {
    const u8* const extData = rpt.GetExtDataPtr();

    Classic::DataFormat cc;
    memcpy(&cc, extData, sizeof(cc));
    key.Decrypt((u8*)&cc, 0, sizeof(cc));
    cc.bt.hex = cc.bt.hex ^ 0xFFFF;

    if (cc.bt.dpad_left)
      display_str += " LEFT";
    if (cc.bt.dpad_right)
      display_str += " RIGHT";
    if (cc.bt.dpad_down)
      display_str += " DOWN";
    if (cc.bt.dpad_up)
      display_str += " UP";
    if (cc.bt.a)
      display_str += " A";
    if (cc.bt.b)
      display_str += " B";
    if (cc.bt.x)
      display_str += " X";
    if (cc.bt.y)
      display_str += " Y";
    if (cc.bt.zl)
      display_str += " ZL";
    if (cc.bt.zr)
      display_str += " ZR";
    if (cc.bt.plus)
      display_str += " +";
    if (cc.bt.minus)
      display_str += " -";
    if (cc.bt.home)
      display_str += " HOME";

    display_str += Analog1DToString(cc.GetLeftTrigger().value, " L", 31);
    display_str += Analog1DToString(cc.GetRightTrigger().value, " R", 31);

    const auto left_stick = cc.GetLeftStick().value;
    display_str += Analog2DToString(left_stick.x, left_stick.y, " ANA", 63);

    const auto right_stick = cc.GetRightStick().value;
    display_str += Analog2DToString(right_stick.x, right_stick.y, " R-ANA", 31);
  }

  return display_str;
}

// NOTE: CPU Thread
void MovieManager::CheckPadStatus(const GCPadStatus* PadStatus, int controllerID)
{
  m_pad_state.A = ((PadStatus->button & PAD_BUTTON_A) != 0);
  m_pad_state.B = ((PadStatus->button & PAD_BUTTON_B) != 0);
  m_pad_state.X = ((PadStatus->button & PAD_BUTTON_X) != 0);
  m_pad_state.Y = ((PadStatus->button & PAD_BUTTON_Y) != 0);
  m_pad_state.Z = ((PadStatus->button & PAD_TRIGGER_Z) != 0);
  m_pad_state.Start = ((PadStatus->button & PAD_BUTTON_START) != 0);

  m_pad_state.DPadUp = ((PadStatus->button & PAD_BUTTON_UP) != 0);
  m_pad_state.DPadDown = ((PadStatus->button & PAD_BUTTON_DOWN) != 0);
  m_pad_state.DPadLeft = ((PadStatus->button & PAD_BUTTON_LEFT) != 0);
  m_pad_state.DPadRight = ((PadStatus->button & PAD_BUTTON_RIGHT) != 0);

  m_pad_state.L = ((PadStatus->button & PAD_TRIGGER_L) != 0);
  m_pad_state.R = ((PadStatus->button & PAD_TRIGGER_R) != 0);
  m_pad_state.TriggerL = PadStatus->triggerLeft;
  m_pad_state.TriggerR = PadStatus->triggerRight;

  m_pad_state.AnalogStickX = PadStatus->stickX;
  m_pad_state.AnalogStickY = PadStatus->stickY;

  m_pad_state.CStickX = PadStatus->substickX;
  m_pad_state.CStickY = PadStatus->substickY;

  m_pad_state.is_connected = PadStatus->isConnected;

  m_pad_state.get_origin = (PadStatus->button & PAD_GET_ORIGIN) != 0;

  m_pad_state.disc = m_has_disc_change;
  m_has_disc_change = false;
  m_pad_state.reset = m_reset;
  m_reset = false;

  {
    std::string display_str = GenerateInputDisplayString(m_pad_state, controllerID);

    std::lock_guard guard(m_input_display_lock);
    m_input_display[controllerID] = std::move(display_str);
  }
}

// NOTE: CPU Thread
void MovieManager::RecordInput(const GCPadStatus* PadStatus, int controllerID)
{
  if (!IsRecordingInput() || !IsUsingPad(controllerID))
    return;

  CheckPadStatus(PadStatus, controllerID);

  m_temp_input.resize(m_current_byte + sizeof(ControllerState));
  memcpy(&m_temp_input[m_current_byte], &m_pad_state, sizeof(ControllerState));
  m_current_byte += sizeof(ControllerState);
}

// NOTE: CPU Thread
void MovieManager::CheckWiimoteStatus(int wiimote, const DataReportBuilder& rpt,
                                      ExtensionNumber ext, const EncryptionKey& key)
{
  {
    std::string display_str = GenerateWiiInputDisplayString(wiimote, rpt, ext, key);

    std::lock_guard guard(m_input_display_lock);
    m_input_display[wiimote + 4] = std::move(display_str);
  }

  if (IsRecordingInput())
    RecordWiimote(wiimote, rpt.GetDataPtr(), rpt.GetDataSize());
}

void MovieManager::RecordWiimote(int wiimote, const u8* data, u8 size)
{
  if (!IsRecordingInput() || !IsUsingWiimote(wiimote))
    return;

  InputUpdate();
  m_temp_input.resize(m_current_byte + size + 1);
  m_temp_input[m_current_byte++] = size;
  memcpy(&m_temp_input[m_current_byte], data, size);
  m_current_byte += size;
}

// NOTE: EmuThread / Host Thread
void MovieManager::ReadHeader()
{
  for (int i = 0; i < 4; ++i)
  {
    if (m_temp_header.GBAControllers & (1 << i))
      m_controllers[i] = ControllerType::GBA;
    else if (m_temp_header.controllers & (1 << i))
      m_controllers[i] = ControllerType::GC;
    else
      m_controllers[i] = ControllerType::None;
    m_wiimotes[i] = (m_temp_header.controllers & (1 << (i + 4))) != 0;
  }
  m_recording_start_time = m_temp_header.recordingStartTime;
  if (m_rerecords < m_temp_header.numRerecords)
    m_rerecords = m_temp_header.numRerecords;

  if (m_temp_header.bSaveConfig)
  {
    m_save_config = true;
    Config::AddLayer(ConfigLoaders::GenerateMovieConfigLoader(&m_temp_header));
    m_clear_save = m_temp_header.bClearSave;
    m_memcards = m_temp_header.memcards;
    m_bongos = m_temp_header.bongos;
    m_net_play = m_temp_header.bNetPlay;
    m_revision = m_temp_header.revision;
  }
  else
  {
    GetSettings();
  }

  m_disc_change_filename = {m_temp_header.discChange.begin(), m_temp_header.discChange.end()};
  m_author = {m_temp_header.author.begin(), m_temp_header.author.end()};
  m_md5 = m_temp_header.md5;
  m_dsp_irom_hash = m_temp_header.DSPiromHash;
  m_dsp_coef_hash = m_temp_header.DSPcoefHash;
}

// NOTE: Host Thread
bool MovieManager::PlayInput(const std::string& movie_path,
                             std::optional<std::string>* savestate_path)
{
  if (m_play_mode != PlayMode::None)
    return false;

  File::IOFile recording_file(movie_path, "rb");
  if (!recording_file.ReadArray(&m_temp_header, 1))
    return false;

  if (!IsMovieHeader(m_temp_header.filetype))
  {
    PanicAlertFmtT("Invalid recording file");
    return false;
  }

  ReadHeader();

  if (AchievementManager::GetInstance().IsHardcoreModeActive())
    return false;

  m_total_frames = m_temp_header.frameCount;
  m_total_lag_count = m_temp_header.lagCount;
  m_total_input_count = m_temp_header.inputCount;
  m_total_tick_count = m_temp_header.tickCount;
  m_current_frame = 0;
  m_current_lag_count = 0;
  m_current_input_count = 0;

  m_play_mode = PlayMode::Playing;

  // Wiimotes cause desync issues if they're not reset before launching the game
  Wiimote::ResetAllWiimotes();

  Core::UpdateWantDeterminism(m_system);

  m_temp_input.resize(recording_file.GetSize() - 256);
  recording_file.ReadBytes(m_temp_input.data(), m_temp_input.size());
  m_current_byte = 0;
  recording_file.Close();

  // Load savestate (and skip to frame data)
  if (m_temp_header.bFromSaveState && savestate_path)
  {
    const std::string savestate_path_temp = movie_path + ".sav";
    if (File::Exists(savestate_path_temp))
    {
      *savestate_path = savestate_path_temp;
    }
    else
    {
      PanicAlertFmtT("Movie {0} indicates that it starts from a savestate, but {1} doesn't exist. "
                     "The movie will likely not sync!",
                     movie_path, savestate_path_temp);
    }

    m_recording_from_save_state = true;

#ifdef USE_RETRO_ACHIEVEMENTS
    // On the chance someone tries to re-enable before the TAS can start
    Config::SetBase(Config::RA_HARDCORE_ENABLED, false);
#endif  // USE_RETRO_ACHIEVEMENTS

    LoadInput(movie_path);
  }

  return true;
}

void MovieManager::DoState(PointerWrap& p)
{
  // many of these could be useful to save even when no movie is active,
  // and the data is tiny, so let's just save it regardless of movie state.
  p.Do(m_current_frame);
  p.Do(m_current_byte);
  p.Do(m_current_lag_count);
  p.Do(m_current_input_count);
  p.Do(m_polled);
  p.Do(m_tick_count_at_last_input);
  // other variables (such as s_totalBytes and m_total_frames) are set in LoadInput
}

// NOTE: Host Thread
void MovieManager::LoadInput(const std::string& movie_path)
{
  File::IOFile t_record;
  if (!t_record.Open(movie_path, "r+b"))
  {
    PanicAlertFmtT("Failed to read {0}", movie_path);
    EndPlayInput(false);
    return;
  }

  t_record.ReadArray(&m_temp_header, 1);

  if (!IsMovieHeader(m_temp_header.filetype))
  {
    PanicAlertFmtT("Savestate movie {0} is corrupted, movie recording stopping...", movie_path);
    EndPlayInput(false);
    return;
  }
  ReadHeader();
  if (!m_read_only)
  {
    m_rerecords++;
    m_temp_header.numRerecords = m_rerecords;
    t_record.Seek(0, File::SeekOrigin::Begin);
    t_record.WriteArray(&m_temp_header, 1);
  }

  ChangePads();
  if (m_system.IsWii())
    ChangeWiiPads(true);

  u64 totalSavedBytes = t_record.GetSize() - 256;

  bool afterEnd = false;
  // This can only happen if the user manually deletes data from the dtm.
  if (m_current_byte > totalSavedBytes)
  {
    PanicAlertFmtT(
        "Warning: You loaded a save whose movie ends before the current frame in the save "
        "(byte {0} < {1}) (frame {2} < {3}). You should load another save before continuing.",
        totalSavedBytes + 256, m_current_byte + 256, m_temp_header.frameCount, m_current_frame);
    afterEnd = true;
  }

  if (!m_read_only || m_temp_input.empty())
  {
    m_total_frames = m_temp_header.frameCount;
    m_total_lag_count = m_temp_header.lagCount;
    m_total_input_count = m_temp_header.inputCount;
    m_total_tick_count = m_tick_count_at_last_input = m_temp_header.tickCount;

    m_temp_input.resize(static_cast<size_t>(totalSavedBytes));
    t_record.ReadBytes(m_temp_input.data(), m_temp_input.size());
  }
  else if (m_current_byte > 0)
  {
    if (m_current_byte > totalSavedBytes)
    {
    }
    else if (m_current_byte > m_temp_input.size())
    {
      afterEnd = true;
      PanicAlertFmtT(
          "Warning: You loaded a save that's after the end of the current movie. (byte {0} "
          "> {1}) (input {2} > {3}). You should load another save before continuing, or load "
          "this state with read-only mode off.",
          m_current_byte + 256, m_temp_input.size() + 256, m_current_input_count,
          m_total_input_count);
    }
    else if (m_current_byte > 0 && !m_temp_input.empty())
    {
      // verify identical from movie start to the save's current frame
      std::vector<u8> movInput(m_current_byte);
      t_record.ReadArray(movInput.data(), movInput.size());

      const auto result = std::mismatch(movInput.begin(), movInput.end(), m_temp_input.begin());

      if (result.first != movInput.end())
      {
        const ptrdiff_t mismatch_index = std::distance(movInput.begin(), result.first);

        // this is a "you did something wrong" alert for the user's benefit.
        // we'll try to say what's going on in excruciating detail, otherwise the user might not
        // believe us.
        if (IsUsingWiimote(0))
        {
          const size_t byte_offset = static_cast<size_t>(mismatch_index) + sizeof(DTMHeader);

          // TODO: more detail
          PanicAlertFmtT("Warning: You loaded a save whose movie mismatches on byte {0} ({1:#x}). "
                         "You should load another save before continuing, or load this state with "
                         "read-only mode off. Otherwise you'll probably get a desync.",
                         byte_offset, byte_offset);

          std::copy(movInput.begin(), movInput.end(), m_temp_input.begin());
        }
        else
        {
          const ptrdiff_t frame = mismatch_index / sizeof(ControllerState);
          ControllerState curPadState;
          memcpy(&curPadState, &m_temp_input[frame * sizeof(ControllerState)],
                 sizeof(ControllerState));
          ControllerState movPadState;
          memcpy(&movPadState, &movInput[frame * sizeof(ControllerState)], sizeof(ControllerState));
          PanicAlertFmtT(
              "Warning: You loaded a save whose movie mismatches on frame {0}. You should load "
              "another save before continuing, or load this state with read-only mode off. "
              "Otherwise you'll probably get a desync.\n\n"
              "More information: The current movie is {1} frames long and the savestate's movie "
              "is {2} frames long.\n\n"
              "On frame {3}, the current movie presses:\n"
              "Start={4}, A={5}, B={6}, X={7}, Y={8}, Z={9}, DUp={10}, DDown={11}, DLeft={12}, "
              "DRight={13}, L={14}, R={15}, LT={16}, RT={17}, AnalogX={18}, AnalogY={19}, CX={20}, "
              "CY={21}, Connected={22}"
              "\n\n"
              "On frame {23}, the savestate's movie presses:\n"
              "Start={24}, A={25}, B={26}, X={27}, Y={28}, Z={29}, DUp={30}, DDown={31}, "
              "DLeft={32}, DRight={33}, L={34}, R={35}, LT={36}, RT={37}, AnalogX={38}, "
              "AnalogY={39}, CX={40}, CY={41}, Connected={42}",
              frame, m_total_frames, m_temp_header.frameCount, frame, curPadState.Start,
              curPadState.A, curPadState.B, curPadState.X, curPadState.Y, curPadState.Z,
              curPadState.DPadUp, curPadState.DPadDown, curPadState.DPadLeft, curPadState.DPadRight,
              curPadState.L, curPadState.R, curPadState.TriggerL, curPadState.TriggerR,
              curPadState.AnalogStickX, curPadState.AnalogStickY, curPadState.CStickX,
              curPadState.CStickY, curPadState.is_connected, frame, movPadState.Start,
              movPadState.A, movPadState.B, movPadState.X, movPadState.Y, movPadState.Z,
              movPadState.DPadUp, movPadState.DPadDown, movPadState.DPadLeft, movPadState.DPadRight,
              movPadState.L, movPadState.R, movPadState.TriggerL, movPadState.TriggerR,
              movPadState.AnalogStickX, movPadState.AnalogStickY, movPadState.CStickX,
              movPadState.CStickY, curPadState.is_connected);
        }
      }
    }
  }
  t_record.Close();

  m_save_config = m_temp_header.bSaveConfig;

  if (!afterEnd)
  {
    if (m_read_only)
    {
      if (m_play_mode != PlayMode::Playing)
      {
        m_play_mode = PlayMode::Playing;
        Core::UpdateWantDeterminism(m_system);
        Core::DisplayMessage("Switched to playback", 2000);
      }
    }
    else
    {
      if (m_play_mode != PlayMode::Recording)
      {
        m_play_mode = PlayMode::Recording;
        Core::UpdateWantDeterminism(m_system);
        Core::DisplayMessage("Switched to recording", 2000);
      }
    }
  }
  else
  {
    EndPlayInput(false);
  }
}

// NOTE: CPU Thread
void MovieManager::CheckInputEnd()
{
  if (m_current_byte >= m_temp_input.size() ||
      (m_system.GetCoreTiming().GetTicks() > m_total_tick_count &&
       !IsRecordingInputFromSaveState()))
  {
    EndPlayInput(!m_read_only);
  }
}

// NOTE: CPU Thread
void MovieManager::PlayController(GCPadStatus* PadStatus, int controllerID)
{
  // Correct playback is entirely dependent on the emulator polling the controllers
  // in the same order done during recording
  if (!IsPlayingInput() || !IsUsingPad(controllerID) || m_temp_input.empty())
    return;

  if (m_current_byte + sizeof(ControllerState) > m_temp_input.size())
  {
    PanicAlertFmtT("Premature movie end in PlayController. {0} + {1} > {2}", m_current_byte,
                   sizeof(ControllerState), m_temp_input.size());
    EndPlayInput(!m_read_only);
    return;
  }

  memcpy(&m_pad_state, &m_temp_input[m_current_byte], sizeof(ControllerState));
  m_current_byte += sizeof(ControllerState);

  PadStatus->isConnected = m_pad_state.is_connected;

  PadStatus->triggerLeft = m_pad_state.TriggerL;
  PadStatus->triggerRight = m_pad_state.TriggerR;

  PadStatus->stickX = m_pad_state.AnalogStickX;
  PadStatus->stickY = m_pad_state.AnalogStickY;

  PadStatus->substickX = m_pad_state.CStickX;
  PadStatus->substickY = m_pad_state.CStickY;

  PadStatus->button = 0;
  PadStatus->button |= PAD_USE_ORIGIN;

  if (m_pad_state.A)
  {
    PadStatus->button |= PAD_BUTTON_A;
    PadStatus->analogA = 0xFF;
  }
  if (m_pad_state.B)
  {
    PadStatus->button |= PAD_BUTTON_B;
    PadStatus->analogB = 0xFF;
  }
  if (m_pad_state.X)
    PadStatus->button |= PAD_BUTTON_X;
  if (m_pad_state.Y)
    PadStatus->button |= PAD_BUTTON_Y;
  if (m_pad_state.Z)
    PadStatus->button |= PAD_TRIGGER_Z;
  if (m_pad_state.Start)
    PadStatus->button |= PAD_BUTTON_START;

  if (m_pad_state.DPadUp)
    PadStatus->button |= PAD_BUTTON_UP;
  if (m_pad_state.DPadDown)
    PadStatus->button |= PAD_BUTTON_DOWN;
  if (m_pad_state.DPadLeft)
    PadStatus->button |= PAD_BUTTON_LEFT;
  if (m_pad_state.DPadRight)
    PadStatus->button |= PAD_BUTTON_RIGHT;

  if (m_pad_state.L)
    PadStatus->button |= PAD_TRIGGER_L;
  if (m_pad_state.R)
    PadStatus->button |= PAD_TRIGGER_R;

  if (m_pad_state.get_origin)
    PadStatus->button |= PAD_GET_ORIGIN;

  if (m_pad_state.disc)
  {
    const Core::CPUThreadGuard guard(m_system);
    if (!m_system.GetDVDInterface().AutoChangeDisc(guard))
    {
      m_system.GetCPU().Break();
      PanicAlertFmtT("Change the disc to {0}", m_disc_change_filename);
    }
  }

  if (m_pad_state.reset)
    m_system.GetProcessorInterface().ResetButton_Tap();

  {
    std::string display_str = GenerateInputDisplayString(m_pad_state, controllerID);

    std::lock_guard guard(m_input_display_lock);
    m_input_display[controllerID] = std::move(display_str);
  }

  CheckInputEnd();
}

// NOTE: CPU Thread
bool MovieManager::PlayWiimote(int wiimote, WiimoteCommon::DataReportBuilder& rpt,
                               ExtensionNumber ext, const EncryptionKey& key)
{
  if (!IsPlayingInput() || !IsUsingWiimote(wiimote) || m_temp_input.empty())
    return false;

  if (m_current_byte > m_temp_input.size())
  {
    PanicAlertFmtT("Premature movie end in PlayWiimote. {0} > {1}", m_current_byte,
                   m_temp_input.size());
    EndPlayInput(!m_read_only);
    return false;
  }

  const u8 size = rpt.GetDataSize();
  const u8 sizeInMovie = m_temp_input[m_current_byte];

  if (size != sizeInMovie)
  {
    PanicAlertFmtT(
        "Fatal desync. Aborting playback. (Error in PlayWiimote: {0} != {1}, byte {2}.){3}",
        sizeInMovie, size, m_current_byte,
        (m_controllers == ControllerTypeArray{}) ?
            " Try re-creating the recording with all GameCube controllers "
            "disabled (in Configure > GameCube > Device Settings)." :
            "");
    EndPlayInput(!m_read_only);
    return false;
  }

  m_current_byte++;

  if (m_current_byte + size > m_temp_input.size())
  {
    PanicAlertFmtT("Premature movie end in PlayWiimote. {0} + {1} > {2}", m_current_byte, size,
                   m_temp_input.size());
    EndPlayInput(!m_read_only);
    return false;
  }

  memcpy(rpt.GetDataPtr(), &m_temp_input[m_current_byte], size);
  m_current_byte += size;

  m_current_input_count++;

  CheckInputEnd();
  return true;
}

// NOTE: Host / EmuThread / CPU Thread
void MovieManager::EndPlayInput(bool cont)
{
  if (cont)
  {
    // If !IsMovieActive(), changing m_play_mode requires calling UpdateWantDeterminism
    ASSERT(IsMovieActive());

    m_play_mode = PlayMode::Recording;
    Core::DisplayMessage("Reached movie end. Resuming recording.", 2000);
  }
  else if (m_play_mode != PlayMode::None)
  {
    // We can be called by EmuThread during boot (CPU::State::PowerDown)
    auto& cpu = m_system.GetCPU();
    const bool was_running = Core::IsRunning(m_system) && !cpu.IsStepping();
    if (was_running && Config::Get(Config::MAIN_MOVIE_PAUSE_MOVIE))
      cpu.Break();
    m_rerecords = 0;
    m_current_byte = 0;
    m_play_mode = PlayMode::None;
    Core::DisplayMessage("Movie End.", 2000);
    m_recording_from_save_state = false;
    Config::RemoveLayer(Config::LayerType::Movie);
    // we don't clear these things because otherwise we can't resume playback if we load a movie
    // state later
    // m_total_frames = s_totalBytes = 0;
    // delete tmpInput;
    // tmpInput = nullptr;

    Core::QueueHostJob([](Core::System& system) { Core::UpdateWantDeterminism(system); });
  }
}

// NOTE: Save State + Host Thread
void MovieManager::SaveRecording(const std::string& filename)
{
  File::IOFile save_record(filename, "wb");
  // Create the real header now and write it
  DTMHeader header;
  memset(&header, 0, sizeof(DTMHeader));

  header.filetype[0] = 'D';
  header.filetype[1] = 'T';
  header.filetype[2] = 'M';
  header.filetype[3] = 0x1A;
  strncpy(header.gameID.data(), SConfig::GetInstance().GetGameID().c_str(), 6);
  header.bWii = m_system.IsWii();
  header.controllers = 0;
  header.GBAControllers = 0;
  for (int i = 0; i < 4; ++i)
  {
    if (IsUsingGBA(i))
      header.GBAControllers |= 1 << i;
    if (IsUsingPad(i))
      header.controllers |= 1 << i;
    if (IsUsingWiimote(i) && m_system.IsWii())
      header.controllers |= 1 << (i + 4);
  }

  header.bFromSaveState = m_recording_from_save_state;
  header.frameCount = m_total_frames;
  header.lagCount = m_total_lag_count;
  header.inputCount = m_total_input_count;
  header.numRerecords = m_rerecords;
  header.recordingStartTime = m_recording_start_time;

  header.bSaveConfig = true;
  ConfigLoaders::SaveToDTM(&header);
  header.memcards = m_memcards;
  header.bClearSave = m_clear_save;
  header.bNetPlay = m_net_play;
  strncpy(header.discChange.data(), m_disc_change_filename.c_str(), header.discChange.size());
  strncpy(header.author.data(), m_author.c_str(), header.author.size());
  header.md5 = m_md5;
  header.bongos = m_bongos;
  header.revision = m_revision;
  header.DSPiromHash = m_dsp_irom_hash;
  header.DSPcoefHash = m_dsp_coef_hash;
  header.tickCount = m_total_tick_count;

  // TODO
  header.uniqueID = 0;
  // header.audioEmulator;

  save_record.WriteArray(&header, 1);

  bool success = save_record.WriteBytes(m_temp_input.data(), m_temp_input.size());

  if (success && m_recording_from_save_state)
  {
    std::string stateFilename = filename + ".sav";
    success = File::CopyRegularFile(File::GetUserPath(D_STATESAVES_IDX) + "dtm.sav", stateFilename);
  }

  if (success)
    Core::DisplayMessage(fmt::format("DTM {} saved", filename), 2000);
  else
    Core::DisplayMessage(fmt::format("Failed to save {}", filename), 2000);
}

// NOTE: GPU Thread
void MovieManager::SetGraphicsConfig()
{
  g_Config.bEFBAccessEnable = m_temp_header.bEFBAccessEnable;
  g_Config.bSkipEFBCopyToRam = m_temp_header.bSkipEFBCopyToRam;
  g_Config.bEFBEmulateFormatChanges = m_temp_header.bEFBEmulateFormatChanges;
  g_Config.bImmediateXFB = m_temp_header.bImmediateXFB;
  g_Config.bSkipXFBCopyToRam = m_temp_header.bSkipXFBCopyToRam;
}

// NOTE: EmuThread / Host Thread
void MovieManager::GetSettings()
{
  using ExpansionInterface::EXIDeviceType;
  const EXIDeviceType slot_a_type = Config::Get(Config::MAIN_SLOT_A);
  const EXIDeviceType slot_b_type = Config::Get(Config::MAIN_SLOT_B);
  const bool slot_a_has_raw_memcard = slot_a_type == EXIDeviceType::MemoryCard;
  const bool slot_a_has_gci_folder = slot_a_type == EXIDeviceType::MemoryCardFolder;
  const bool slot_b_has_raw_memcard = slot_b_type == EXIDeviceType::MemoryCard;
  const bool slot_b_has_gci_folder = slot_b_type == EXIDeviceType::MemoryCardFolder;

  m_save_config = true;
  m_net_play = NetPlay::IsNetPlayRunning();
  if (m_system.IsWii())
  {
    u64 title_id = SConfig::GetInstance().GetTitleID();
    m_clear_save = !File::Exists(
        Common::GetTitleDataPath(title_id, Common::FromWhichRoot::Session) + "/banner.bin");
  }
  else
  {
    const auto raw_memcard_exists = [](ExpansionInterface::Slot card_slot) {
      return File::Exists(Config::GetMemcardPath(card_slot, SConfig::GetInstance().m_region));
    };
    const auto gci_folder_has_saves = [this](ExpansionInterface::Slot card_slot) {
      const auto [path, migrate] = ExpansionInterface::CEXIMemoryCard::GetGCIFolderPath(
          card_slot, ExpansionInterface::AllowMovieFolder::No, *this);
      const u64 number_of_saves = File::ScanDirectoryTree(path, false).size;
      return number_of_saves > 0;
    };

    m_clear_save = !(slot_a_has_raw_memcard && raw_memcard_exists(ExpansionInterface::Slot::A)) &&
                   !(slot_b_has_raw_memcard && raw_memcard_exists(ExpansionInterface::Slot::B)) &&
                   !(slot_a_has_gci_folder && gci_folder_has_saves(ExpansionInterface::Slot::A)) &&
                   !(slot_b_has_gci_folder && gci_folder_has_saves(ExpansionInterface::Slot::B));
  }
  m_memcards |= (slot_a_has_raw_memcard || slot_a_has_gci_folder) << 0;
  m_memcards |= (slot_b_has_raw_memcard || slot_b_has_gci_folder) << 1;

  m_revision = ConvertGitRevisionToBytes(Common::GetScmRevGitStr());

  if (!Config::Get(Config::MAIN_DSP_HLE))
  {
    std::string irom_file = File::GetUserPath(D_GCUSER_IDX) + DSP_IROM;
    std::string coef_file = File::GetUserPath(D_GCUSER_IDX) + DSP_COEF;

    if (!File::Exists(irom_file))
      irom_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_IROM;
    if (!File::Exists(coef_file))
      coef_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_COEF;
    std::vector<u16> irom(DSP::DSP_IROM_SIZE);
    File::IOFile file_irom(irom_file, "rb");

    file_irom.ReadArray(irom.data(), irom.size());
    file_irom.Close();
    for (u16& entry : irom)
      entry = Common::swap16(entry);

    std::vector<u16> coef(DSP::DSP_COEF_SIZE);
    File::IOFile file_coef(coef_file, "rb");

    file_coef.ReadArray(coef.data(), coef.size());
    file_coef.Close();
    for (u16& entry : coef)
      entry = Common::swap16(entry);
    m_dsp_irom_hash =
        Common::HashAdler32(reinterpret_cast<u8*>(irom.data()), DSP::DSP_IROM_BYTE_SIZE);
    m_dsp_coef_hash =
        Common::HashAdler32(reinterpret_cast<u8*>(coef.data()), DSP::DSP_COEF_BYTE_SIZE);
  }
  else
  {
    m_dsp_irom_hash = 0;
    m_dsp_coef_hash = 0;
  }
}

// NOTE: Entrypoint for own thread
void MovieManager::CheckMD5()
{
  if (m_current_file_name.empty())
    return;

  // The MD5 hash was introduced in 3.0-846-gca650d4435.
  // Before that, these header bytes were set to zero.
  if (m_temp_header.md5 == std::array<u8, 16>{})
    return;

  Core::DisplayMessage("Verifying checksum...", 2000);

  std::array<u8, 16> game_md5;
  mbedtls_md_file(mbedtls_md_info_from_type(MBEDTLS_MD_MD5), m_current_file_name.c_str(),
                  game_md5.data());

  if (game_md5 == m_md5)
    Core::DisplayMessage("Checksum of current game matches the recorded game.", 2000);
  else
    Core::DisplayMessage("Checksum of current game does not match the recorded game!", 3000);
}

// NOTE: Entrypoint for own thread
void MovieManager::GetMD5()
{
  if (m_current_file_name.empty())
    return;

  Core::DisplayMessage("Calculating checksum of game file...", 2000);
  mbedtls_md_file(mbedtls_md_info_from_type(MBEDTLS_MD_MD5), m_current_file_name.c_str(),
                  m_md5.data());
  Core::DisplayMessage("Finished calculating checksum.", 2000);
}

// NOTE: EmuThread
void MovieManager::Shutdown()
{
  m_current_input_count = m_total_input_count = m_total_frames = m_tick_count_at_last_input = 0;
  m_temp_input.clear();
}
}  // namespace Movie
