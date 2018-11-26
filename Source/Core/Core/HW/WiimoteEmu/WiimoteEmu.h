// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <string>

#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

// Registry sizes
#define WIIMOTE_EEPROM_SIZE (16 * 1024)
#define WIIMOTE_EEPROM_FREE_SIZE 0x1700

class PointerWrap;

namespace ControllerEmu
{
class BooleanSetting;
class Buttons;
class ControlGroup;
class Cursor;
class Extension;
class Force;
class ModifySettingsButton;
class NumericSetting;
class Output;
class Tilt;
}  // namespace ControllerEmu

namespace WiimoteReal
{
class Wiimote;
}
namespace WiimoteEmu
{
enum class WiimoteGroup
{
  Buttons,
  DPad,
  Shake,
  IR,
  Tilt,
  Swing,
  Rumble,
  Extension,

  Options,
  Hotkeys
};

enum
{
  EXT_NONE,

  EXT_NUNCHUK,
  EXT_CLASSIC,
  EXT_GUITAR,
  EXT_DRUMS,
  EXT_TURNTABLE
};

enum class NunchukGroup
{
  Buttons,
  Stick,
  Tilt,
  Swing,
  Shake
};

enum class ClassicGroup
{
  Buttons,
  Triggers,
  DPad,
  LeftStick,
  RightStick
};

enum class GuitarGroup
{
  Buttons,
  Frets,
  Strum,
  Whammy,
  Stick,
  SliderBar
};

enum class DrumsGroup
{
  Buttons,
  Pads,
  Stick
};

enum class TurntableGroup
{
  Buttons,
  Stick,
  EffectDial,
  LeftTable,
  RightTable,
  Crossfade
};
#pragma pack(push, 1)

struct ReportFeatures
{
  // Byte counts:
  // Features are always in the following order in an input report:
  u8 core_size, accel_size, ir_size, ext_size, total_size;

  int GetCoreOffset() const { return 2; }
  int GetAccelOffset() const { return GetCoreOffset() + core_size; }
  int GetIROffset() const { return GetAccelOffset() + accel_size; }
  int GetExtOffset() const { return GetIROffset() + ir_size; }
};

struct AccelData
{
  double x, y, z;
};

// Used for a dynamic swing or
// shake
struct DynamicData
{
  std::array<int, 3> timing;                 // Hold length in frames for each axis
  std::array<double, 3> intensity;           // Swing or shake intensity
  std::array<int, 3> executing_frames_left;  // Number of frames to execute the intensity operation
};

// Used for a dynamic swing or
// shake.  This is used to pass
// in data that defines the dynamic
// action
struct DynamicConfiguration
{
  double low_intensity;
  int frames_needed_for_low_intensity;

  double med_intensity;
  // Frames needed for med intensity can be calculated between high & low

  double high_intensity;
  int frames_needed_for_high_intensity;

  int frames_to_execute;  // How many frames should we execute the action for?
};

struct ADPCMState
{
  s32 predictor, step;
};

struct ExtensionReg
{
  // 16 bytes of possible extension data
  u8 controller_data[0x10];

  u8 unknown2[0x10];

  // address 0x20
  u8 calibration[0x10];
  u8 unknown3[0x10];

  // address 0x40
  u8 encryption_key[0x10];
  u8 unknown4[0xA0];

  // address 0xF0
  u8 encryption;
  u8 unknown5[0x9];

  // address 0xFA
  u8 constant_id[6];
};
#pragma pack(pop)

static_assert(0x100 == sizeof(ExtensionReg));

void UpdateCalibrationDataChecksum(std::array<u8, 0x10>& data);

void EmulateShake(AccelData* accel, ControllerEmu::Buttons* buttons_group, double intensity,
                  u8* shake_step);

void EmulateDynamicShake(AccelData* accel, DynamicData& dynamic_data,
                         ControllerEmu::Buttons* buttons_group, const DynamicConfiguration& config,
                         u8* shake_step);

void EmulateTilt(AccelData* accel, ControllerEmu::Tilt* tilt_group, bool sideways = false,
                 bool upright = false);

void EmulateSwing(AccelData* accel, ControllerEmu::Force* swing_group, double intensity,
                  bool sideways = false, bool upright = false);

void EmulateDynamicSwing(AccelData* accel, DynamicData& dynamic_data,
                         ControllerEmu::Force* swing_group, const DynamicConfiguration& config,
                         bool sideways = false, bool upright = false);

enum
{
  ACCEL_ZERO_G = 0x80,
  ACCEL_ONE_G = 0x9A,
  ACCEL_RANGE = (ACCEL_ONE_G - ACCEL_ZERO_G),
};

class I2CSlave
{
public:
  virtual ~I2CSlave() = default;

  virtual int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) = 0;
  virtual int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) = 0;

  template <typename T>
  static int RawRead(T* reg_data, u8 addr, int count, u8* data_out)
  {
    static_assert(std::is_pod<T>::value);
    static_assert(0x100 == sizeof(T));

    // TODO: addr wraps around after 0xff

    u8* src = reinterpret_cast<u8*>(reg_data) + addr;
    count = std::min(count, int(reinterpret_cast<u8*>(reg_data + 1) - src));

    std::copy_n(src, count, data_out);

    return count;
  }

  template <typename T>
  static int RawWrite(T* reg_data, u8 addr, int count, const u8* data_in)
  {
    static_assert(std::is_pod<T>::value);
    static_assert(0x100 == sizeof(T));

    // TODO: addr wraps around after 0xff

    u8* dst = reinterpret_cast<u8*>(reg_data) + addr;
    count = std::min(count, int(reinterpret_cast<u8*>(reg_data + 1) - dst));

    std::copy_n(data_in, count, dst);

    return count;
  }
};

class I2CBus
{
public:
  void AddSlave(I2CSlave* slave) { m_slaves.emplace_back(slave); }

  void RemoveSlave(I2CSlave* slave)
  {
    m_slaves.erase(std::remove(m_slaves.begin(), m_slaves.end(), slave), m_slaves.end());
  }

  void Reset() { m_slaves.clear(); }

  // TODO: change int to u16 or something
  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
  {
    // TODO: the real bus seems to read in blocks of 8.
    // peripherals NACK after each 8 bytes.

    // INFO_LOG(WIIMOTE, "i2c bus read: 0x%02x @ 0x%02x (%d)", slave_addr, addr, count);
    for (auto& slave : m_slaves)
    {
      auto const bytes_read = slave->BusRead(slave_addr, addr, count, data_out);

      // A slave responded, we are done.
      if (bytes_read)
        return bytes_read;
    }

    return 0;
  }

  // TODO: change int to u16 or something
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
  {
    // TODO: write in blocks of 6 to simulate the real bus
    // sometimes it writes in blocks of 8, (speaker data)
    // this might trigger activation writes more accurately

    // INFO_LOG(WIIMOTE, "i2c bus write: 0x%02x @ 0x%02x (%d)", slave_addr, addr, count);
    for (auto& slave : m_slaves)
    {
      auto const bytes_written = slave->BusWrite(slave_addr, addr, count, data_in);

      // A slave responded, we are done.
      if (bytes_written)
        return bytes_written;
    }

    return 0;
  }

private:
  std::vector<I2CSlave*> m_slaves;
};

class ExtensionAttachment : public I2CSlave
{
public:
  virtual bool ReadDeviceDetectPin() = 0;
};

class ExtensionPort
{
public:
  ExtensionPort(I2CBus& _i2c_bus) : m_i2c_bus(_i2c_bus) {}

  // Simulates the "device-detect" pin.
  // Wiimote uses this to detect extension change..
  // and then send a status report..
  bool IsDeviceConnected()
  {
    if (m_attachment)
      return m_attachment->ReadDeviceDetectPin();
    else
      return false;
  }

  void SetAttachment(ExtensionAttachment* dev)
  {
    m_i2c_bus.RemoveSlave(m_attachment);
    m_i2c_bus.AddSlave(m_attachment = dev);
  }

private:
  ExtensionAttachment* m_attachment;
  I2CBus& m_i2c_bus;
};

class Wiimote : public ControllerEmu::EmulatedController
{
  friend class WiimoteReal::Wiimote;

public:
  enum
  {
    PAD_LEFT = 0x01,
    PAD_RIGHT = 0x02,
    PAD_DOWN = 0x04,
    PAD_UP = 0x08,
    BUTTON_PLUS = 0x10,

    BUTTON_TWO = 0x0100,
    BUTTON_ONE = 0x0200,
    BUTTON_B = 0x0400,
    BUTTON_A = 0x0800,
    BUTTON_MINUS = 0x1000,
    BUTTON_HOME = 0x8000,
  };

  explicit Wiimote(unsigned int index);
  std::string GetName() const override;
  ControllerEmu::ControlGroup* GetWiimoteGroup(WiimoteGroup group);
  ControllerEmu::ControlGroup* GetNunchukGroup(NunchukGroup group);
  ControllerEmu::ControlGroup* GetClassicGroup(ClassicGroup group);
  ControllerEmu::ControlGroup* GetGuitarGroup(GuitarGroup group);
  ControllerEmu::ControlGroup* GetDrumsGroup(DrumsGroup group);
  ControllerEmu::ControlGroup* GetTurntableGroup(TurntableGroup group);

  void Update();
  void InterruptChannel(u16 channel_id, const void* data, u32 size);
  void ControlChannel(u16 channel_id, const void* data, u32 size);
  void SpeakerData(const u8* data, int length);
  bool CheckForButtonPress();
  void Reset();

  void DoState(PointerWrap& p);
  void RealState();

  void LoadDefaults(const ControllerInterface& ciface) override;

  int CurrentExtension() const;

protected:
  bool Step();
  void HidOutputReport(const wm_report* sr, bool send_ack = true);
  void HandleExtensionSwap();
  void UpdateButtonsStatus();

  void GetButtonData(u8* data);
  void GetAccelData(s16* x, s16* y, s16* z);
  void UpdateIRData(bool use_accel);

private:
  I2CBus m_i2c_bus;

  ExtensionPort m_extension_port{m_i2c_bus};

  struct IRCameraLogic : public I2CSlave
  {
    // TODO: some of this memory is write-only and should return error 7.

    struct RegData
    {
      // Contains sensitivity and other unknown data
      u8 data[0x33];
      u8 mode;
      u8 unk[3];
      // addr 0x37
      u8 camera_data[36];
      u8 unk2[165];
    } reg_data;

    static_assert(0x100 == sizeof(reg_data));

    static const u8 DEVICE_ADDR = 0x58;

    int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      return RawRead(&reg_data, addr, count, data_out);
    }

    int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      return RawWrite(&reg_data, addr, count, data_in);
    }

  } m_camera_logic;

  class ExtensionLogic : public ExtensionAttachment
  {
  public:
    ExtensionReg reg_data;
    wiimote_key ext_key;

    ControllerEmu::Extension* extension;

    static const u8 DEVICE_ADDR = 0x52;

    void Update();

  private:
    int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      if (0x00 == addr)
      {
        // Here we should sample user input and update controller data
        // Moved into Update() function for TAS determinism
        // TAS code fails to sync data reads and such..
        // extension->GetState(reg_data.controller_data);
      }

      auto const result = RawRead(&reg_data, addr, count, data_out);

      // Encrypt data read from extension register
      // Check if encrypted reads is on
      if (0xaa == reg_data.encryption)
      {
        INFO_LOG(WIIMOTE, "Encrypted read.");
        WiimoteEncrypt(&ext_key, data_out, addr, (u8)count);
      }
      else
      {
        INFO_LOG(WIIMOTE, "Unencrypted read.");
      }

      return result;
    }

    int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      auto const result = RawWrite(&reg_data, addr, count, data_in);

      if (addr + count > 0x40 && addr < 0x50)
      {
        // Run the key generation on all writes in the key area, it doesn't matter
        //  that we send it parts of a key, only the last full key will have an effect
        WiimoteGenerateKey(&ext_key, reg_data.encryption_key);
      }

      return result;
    }

    bool ReadDeviceDetectPin() override;

  } m_ext_logic;

  struct SpeakerLogic : public I2CSlave
  {
    static const u8 DATA_FORMAT_ADPCM = 0x00;
    static const u8 DATA_FORMAT_PCM = 0x40;

    // TODO: It seems reading address 0x00 should always return 0xff.

    struct
    {
      // Speaker reports result in a write of samples to addr 0x00 (which also plays sound)
      u8 speaker_data;
      u8 unk_1;
      u8 format;
      // seems to always play at 6khz no matter what this is set to?
      // or maybe it only applies to pcm input
      u16 sample_rate;
      u8 volume;
      u8 unk_6;
      u8 unk_7;
      u8 play;
      u8 unk_9;
      u8 unknown[0xf4];
    } reg_data;

    static_assert(0x100 == sizeof(reg_data));

    ADPCMState adpcm_state;

    static const u8 DEVICE_ADDR = 0x51;

    int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      return RawRead(&reg_data, addr, count, data_out);
    }

    int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override
    {
      if (DEVICE_ADDR != slave_addr)
        return 0;

      if (0x00 == addr)
      {
        ERROR_LOG(WIIMOTE, "Writing of speaker data to address 0x00 is unimplemented!");
        return count;
      }
      else
      {
        // TODO: should address wrap around after 0xff result in processing speaker data?
        return RawWrite(&reg_data, addr, count, data_in);
      }
    }

  } m_speaker_logic;

  struct MotionPlusLogic : public ExtensionAttachment
  {
    // The bus on the end of the motion plus:
    I2CBus i2c_bus;

    // The port on the end of the motion plus:
    ExtensionPort extension_port{i2c_bus};

    void Update();

#pragma pack(push, 1)
    struct MotionPlusRegister
    {
      u8 controller_data[21];
      u8 unknown[11];

      // address 0x20
      u8 calibration_data[0x20];
      u8 unknown2[0xb0];

      // address 0xF0
      u8 initialized;

      u8 unknown3[6];

      // address 0xf7
      // Wii Sports Resort reads regularly and claims mplus is disconnected if not to its liking
      // Value starts at 0x00 and goes up after activation (not initialization)
      // Immediately returns 0x02, even still after 15 and 30 seconds
      u8 initialization_status;

      u8 unknown4[2];

      // address 0xFA
      u8 ext_identifier[6];
    } reg_data;
#pragma pack(pop)

    static_assert(0x100 == sizeof(reg_data));

  private:
    static const u8 INACTIVE_DEVICE_ADDR = 0x53;
    static const u8 ACTIVE_DEVICE_ADDR = 0x52;

    enum class PassthroughMode : u8
    {
      PASSTHROUGH_DISABLED = 0x04,
      PASSTHROUGH_NUNCHUK = 0x05,
      PASSTHROUGH_CLASSIC = 0x07,
    };

    // TODO: savestate
    u8 times_updated_since_activation = 0;

    bool IsActive() const { return ACTIVE_DEVICE_ADDR << 1 == reg_data.ext_identifier[2]; }

    PassthroughMode GetPassthroughMode() const
    {
      return static_cast<PassthroughMode>(reg_data.ext_identifier[4]);
    }

    // TODO: when activated it seems the motion plus reactivates the extension
    // It sends 0x55 to 0xf0
    // It also writes 0x00 to slave:0x52 addr:0xfa for some reason
    // And starts a write to 0xfa but never writes bytes..
    // It tries to read data at 0x00 for 3 times (failing)
    // then it reads the 16 bytes of calibration at 0x20 and stops

    // TODO: if an extension is attached after activation, it also does this.

    int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override
    {
      if (IsActive())
      {
        // Motion plus does not respond to 0x53 when activated
        if (ACTIVE_DEVICE_ADDR == slave_addr)
          return RawRead(&reg_data, addr, count, data_out);
        else
          return 0;
      }
      else
      {
        if (INACTIVE_DEVICE_ADDR == slave_addr)
          return RawRead(&reg_data, addr, count, data_out);
        else
        {
          // Passthrough to the connected extension (if any)
          return i2c_bus.BusRead(slave_addr, addr, count, data_out);
        }
      }
    }

    int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override
    {
      if (IsActive())
      {
        // Motion plus does not respond to 0x53 when activated
        if (ACTIVE_DEVICE_ADDR == slave_addr)
        {
          auto const result = RawWrite(&reg_data, addr, count, data_in);
          return result;

          // It seems a write of any value triggers deactivation.
          if (0xf0 == addr)
          {
            // Deactivate motion plus:
            reg_data.ext_identifier[2] = INACTIVE_DEVICE_ADDR << 1;
            times_updated_since_activation = 0;
          }
        }
        else
        {
          // No i2c passthrough when activated.
          return 0;
        }
      }
      else
      {
        if (INACTIVE_DEVICE_ADDR == slave_addr)
        {
          auto const result = RawWrite(&reg_data, addr, count, data_in);

          // It seems a write of any value triggers activation.
          if (0xfe == addr)
          {
            INFO_LOG(WIIMOTE, "Motion Plus has been activated with value: %d", data_in[0]);

            // Activate motion plus:
            reg_data.ext_identifier[2] = ACTIVE_DEVICE_ADDR << 1;
            reg_data.initialization_status = 0x2;

            // Some hax to disable encryption:
            std::array<u8, 1> data = {0x55};
            i2c_bus.BusWrite(ACTIVE_DEVICE_ADDR, 0xf0, (int)data.size(), data.data());
          }

          return result;
        }
        else
        {
          // Passthrough to the connected extension (if any)
          return i2c_bus.BusWrite(slave_addr, addr, count, data_in);
        }
      }
    }

  private:
    bool ReadDeviceDetectPin() override
    {
      if (IsActive())
      {
        // TODO: logic for when motion plus deactivates
        return true;
      }
      else
      {
        return extension_port.IsDeviceConnected();
      }
    }
  } m_motion_plus_logic;

  void ReportMode(const wm_report_mode* dr);
  void SendAck(u8 report_id, u8 error_code = 0x0);
  void RequestStatus(const wm_request_status* rs = nullptr);
  void ReadData(const wm_read_data* rd);
  void WriteData(const wm_write_data* wd);
  bool ProcessReadDataRequest();
  bool NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size, u8 reporting_mode);

  // control groups
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::Buttons* m_shake;
  ControllerEmu::Buttons* m_shake_soft;
  ControllerEmu::Buttons* m_shake_hard;
  ControllerEmu::Buttons* m_shake_dynamic;
  ControllerEmu::Cursor* m_ir;
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::Force* m_swing_slow;
  ControllerEmu::Force* m_swing_fast;
  ControllerEmu::Force* m_swing_dynamic;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Output* m_motor;
  ControllerEmu::Extension* m_extension;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::BooleanSetting* m_sideways_setting;
  ControllerEmu::BooleanSetting* m_upright_setting;
  ControllerEmu::NumericSetting* m_battery_setting;
  ControllerEmu::ModifySettingsButton* m_hotkeys;

  DynamicData m_swing_dynamic_data;
  DynamicData m_shake_dynamic_data;

  // Wiimote accel data
  // TODO: can this member be eliminated?
  AccelData m_accel;

  // Wiimote index, 0-3
  const u8 m_index;

  double ir_sin, ir_cos;  // for the low pass filter

  bool m_rumble_on;
  bool m_speaker_mute;

  bool m_reporting_auto;
  u8 m_reporting_mode;
  u16 m_reporting_channel;

  std::array<u8, 3> m_shake_step{};
  std::array<u8, 3> m_shake_soft_step{};
  std::array<u8, 3> m_shake_hard_step{};
  std::array<u8, 3> m_shake_dynamic_step{};

  bool m_sensor_bar_on_top;

  wm_status_report m_status;

  struct ReadRequest
  {
    u8 space;
    u8 slave_address;
    u16 address;
    u16 size;
  } m_read_request;

  u8 m_eeprom[WIIMOTE_EEPROM_SIZE];
};
}  // namespace WiimoteEmu
