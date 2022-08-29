// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WII_IPC.h"

#include <array>
#include <bitset>
#include <string_view>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/IOS/IOS.h"
#include "Core/System.h"

// This is the intercommunication between ARM and PPC. Currently only PPC actually uses it, because
// of the IOS HLE
// How IOS uses IPC:
// X1 Execute command: a new pointer is available in HW_IPC_PPCCTRL
// X2 Reload (a new IOS is being loaded, old one doesn't need to reply anymore)
// Y1 Command executed and reply available in HW_IPC_ARMMSG
// Y2 Command acknowledge
// m_ppc_msg is a pointer to 0x40byte command structure
// m_arm_msg is, similarly, starlet's response buffer*

namespace IOS
{
enum
{
  IPC_PPCMSG = 0x00,
  IPC_PPCCTRL = 0x04,
  IPC_ARMMSG = 0x08,
  IPC_ARMCTRL = 0x0c,

  VI1CFG = 0x18,
  VIDIM = 0x1c,
  VISOLID = 0x24,

  PPC_IRQFLAG = 0x30,
  PPC_IRQMASK = 0x34,
  ARM_IRQFLAG = 0x38,
  ARM_IRQMASK = 0x3c,

  SRNPROT = 0x60,
  AHBPROT = 0x64,

  // Broadway GPIO access. We don't currently implement the interrupts.
  GPIOB_OUT = 0xc0,
  GPIOB_DIR = 0xc4,
  GPIOB_IN = 0xc8,
  GPIOB_INTLVL = 0xcc,
  GPIOB_INTFLAG = 0xd0,
  GPIOB_INTMASK = 0xd4,
  GPIOB_STRAPS = 0xd8,
  // Starlet GPIO access. We emulate some of these for /dev/di.
  GPIO_ENABLE = 0xdc,
  GPIO_OUT = 0xe0,
  GPIO_DIR = 0xe4,
  GPIO_IN = 0xe8,
  GPIO_INTLVL = 0xec,
  GPIO_INTFLAG = 0xf0,
  GPIO_INTMASK = 0xf4,
  GPIO_STRAPS = 0xf8,
  GPIO_OWNER = 0xfc,

  COMPAT = 0x180,
  RESETS = 0x194,

  UNK_1CC = 0x1cc,
  UNK_1D0 = 0x1d0,
};

// Indicates which pins are accessible by broadway.  Writable by starlet only.
static constexpr Common::Flags<GPIO> gpio_owner = {GPIO::SLOT_LED, GPIO::SLOT_IN, GPIO::SENSOR_BAR,
                                                   GPIO::DO_EJECT, GPIO::AVE_SCL, GPIO::AVE_SDA};

static u32 resets;

#pragma pack(1)
struct AVEState
{
  // See https://wiibrew.org/wiki/Hardware/AV_Encoder#Registers_description
  // (note that the code snippet indicates that values are big-endian)
  u8 timings;                             // 0x00
  u8 video_output_config;                 // 0x01
  u8 vertical_blanking_interval_control;  // 0x02
  u8 composite_trap_filter_control;       // 0x03
  u8 audio_video_output_control;          // 0x04
  u8 cgms_high, cgms_low;                 // 0x05-0x06
  u8 pad1;                                // 0x07
  u8 wss_high, wss_low;                   // 0x08-0x09, Widescreen signaling?
  u8 rgb_color_output_control;            // 0x0A, only used when video_output_config is DEBUG (3)?
  std::array<u8, 5> pad2;                 // 0x0B-0x0F
  std::array<u8, 33> gamma_coefficients;  // 0x10-0x30
  std::array<u8, 15> pad3;                // 0x31-0x3F
  std::array<u8, 26> macrovision_code;    // 0x40-0x59, analog copy protection
  std::array<u8, 8> pad4;                 // 0x5A-0x61
  u8 rgb_switch;                          // 0x62, swap blue and red channels
  std::array<u8, 2> pad5;                 // 0x63-0x64
  u8 color_dac;                           // 0x65
  u8 pad6;                                // 0x66
  u8 color_test;                          // 0x67, display a color test pattern
  std::array<u8, 2> pad7;                 // 0x68-0x69
  u8 ccsel;                               // 0x6A
  std::array<u8, 2> pad8;                 // 0x6B-0x6C
  u8 mute;                                // 0x6D
  u8 rgb_output_filter;                   // 0x6E
  std::array<u8, 2> pad9;                 // 0x6F-0x70
  u8 right_volume;                        // 0x71
  u8 left_volume;                         // 0x72
  std::array<u8, 7> pad10;                // 0x73-0x79
  std::array<u8, 4> closed_captioning;    // 0x7A-0x7D
  std::array<u8, 130> pad11;              // 0x7E-0xFF
};
#pragma pack()
static_assert(sizeof(AVEState) == 0x100);
static AVEState ave_state;
static std::bitset<sizeof(AVEState)> ave_ever_logged;  // For logging only; not saved

// An I²C bus implementation accessed via bit-banging.
// A few assumptions and limitations exist:
// - All devices support both writes and reads.
// - Timing is not implemented at all; the clock signal can be changed as fast as needed.
// - Devices are not allowed to stretch the clock signal. (Nintendo's write code does not seem to
// implement clock stretching in any case, though some homebrew does.)
// - All devices use a 1-byte auto-incrementing address which wraps around from 255 to 0.
// - The device address is handled by this I2CBus class, instead of the device itself.
// - The device address is set on writes, and re-used for reads; writing an address and data and
// then switching to reading uses the incremented address. Every write must specify the address.
// - Reading without setting the device address beforehand is disallowed; the I²C specification
// allows such reads but does not specify how they behave (or anything about the behavior of the
// device address).
// - Switching between multiple devices using a restart does not reset the device address; the
// device address is only reset on stopping. This means that a write to one device followed by a
// read from a different device would result in reading from the last used device address (without
// any warning).
// - 10-bit addressing and other reserved addressing modes are not implemented.
class I2CBus
{
public:
  enum class State
  {
    Inactive,
    Activating,
    SetI2CAddress,
    WriteDeviceAddress,
    WriteToDevice,
    ReadFromDevice,
  };

  State state;
  u8 bit_counter;
  u8 current_byte;
  std::optional<u8> i2c_address;  // Not shifted; includes the read flag
  std::optional<u8> device_address;

  void Update(Core::System& system, const bool old_scl, const bool new_scl, const bool old_sda,
              const bool new_sda);
  bool GetSCL() const;
  bool GetSDA() const;

private:
  void Start();
  void Stop();
  void SCLRisingEdge(const bool sda);
  void SCLFallingEdge(const bool sda);
  bool WriteExpected() const;
};
I2CBus i2c_state;

static CoreTiming::EventType* updateInterrupts;

WiiIPC::WiiIPC(Core::System& system) : m_system(system)
{
}

WiiIPC::~WiiIPC() = default;

void WiiIPC::DoState(PointerWrap& p)
{
  p.Do(m_ppc_msg);
  p.Do(m_arm_msg);
  p.Do(m_ctrl);
  p.Do(m_ppc_irq_flags);
  p.Do(m_ppc_irq_masks);
  p.Do(m_arm_irq_flags);
  p.Do(m_arm_irq_masks);
  p.Do(m_gpio_dir);
  p.Do(m_gpio_out);
  p.Do(m_resets);
}

void WiiIPC::InitState()
{
  m_ctrl = CtrlRegister();
  m_ppc_msg = 0;
  m_arm_msg = 0;

  m_ppc_irq_flags = 0;
  m_ppc_irq_masks = 0;
  m_arm_irq_flags = 0;
  m_arm_irq_masks = 0;

  // The only inputs are POWER, EJECT_BTN, SLOT_IN, EEP_MISO, and sometimes AVE_SCL and AVE_SDA;
  // Broadway only has access to SLOT_IN, AVE_SCL, and AVE_SDA.
  m_gpio_dir = {
      GPIO::POWER,      GPIO::SHUTDOWN, GPIO::FAN,    GPIO::DC_DC,   GPIO::DI_SPIN,  GPIO::SLOT_LED,
      GPIO::SENSOR_BAR, GPIO::DO_EJECT, GPIO::EEP_CS, GPIO::EEP_CLK, GPIO::EEP_MOSI, GPIO::AVE_SCL,
      GPIO::AVE_SDA,    GPIO::DEBUG0,   GPIO::DEBUG1, GPIO::DEBUG2,  GPIO::DEBUG3,   GPIO::DEBUG4,
      GPIO::DEBUG5,     GPIO::DEBUG6,   GPIO::DEBUG7,
  };
  m_gpio_out = {};

  // A cleared bit indicates the device is reset/off, so set everything to 1 (this may not exactly
  // match hardware)
  m_resets = 0xffffffff;

  m_ppc_irq_masks |= INT_CAUSE_IPC_BROADWAY;

  i2c_state = {};
  ave_state = {};
  ave_state.video_output_config = 0x23;
  ave_ever_logged.reset();
}

void WiiIPC::Init()
{
  InitState();
  m_event_type_update_interrupts =
      m_system.GetCoreTiming().RegisterEvent("IPCInterrupt", UpdateInterruptsCallback);
}

void WiiIPC::Reset()
{
  INFO_LOG_FMT(WII_IPC, "Resetting ...");
  InitState();
}

void WiiIPC::Shutdown()
{
}

static std::string_view GetAVERegisterName(u8 address)
{
  if (address == 0x00)
    return "A/V Timings";
  else if (address == 0x01)
    return "Video Output configuration";
  else if (address == 0x02)
    return "Vertical blanking interval (VBI) control";
  else if (address == 0x03)
    return "Composite Video Trap Filter control";
  else if (address == 0x04)
    return "A/V output control";
  else if (address == 0x05 || address == 0x06)
    return "CGMS protection";
  else if (address == 0x08 || address == 0x09)
    return "WSS (Widescreen signaling)";
  else if (address == 0x0A)
    return "RGB color output control";
  else if (address >= 0x10 && address <= 0x30)
    return "Gamma coefficients";
  else if (address >= 0x40 && address <= 0x59)
    return "Macrovision code";
  else if (address == 0x62)
    return "RGB switch control";
  else if (address == 0x65)
    return "Color DAC control";
  else if (address == 0x67)
    return "Color Test";
  else if (address == 0x6A)
    return "CCSEL";
  else if (address == 0x6D)
    return "Audio mute control";
  else if (address == 0x6E)
    return "RGB output filter";
  else if (address == 0x71)
    return "Audio stereo output control - right volume";
  else if (address == 0x72)
    return "Audio stereo output control - right volume";
  else if (address >= 0x7a && address <= 0x7d)
    return "Closed Captioning control";
  else
    return fmt::format("Unknown ({:02x})", address);
}

static u32 ReadGPIOIn(Core::System& system)
{
  Common::Flags<GPIO> gpio_in;
  gpio_in[GPIO::SLOT_IN] = system.GetDVDInterface().IsDiscInside();
  gpio_in[GPIO::AVE_SCL] = i2c_state.GetSCL();
  gpio_in[GPIO::AVE_SDA] = i2c_state.GetSDA();
  return gpio_in.m_hex;
}

bool I2CBus::GetSCL() const
{
  return true;  // passive pullup - no clock stretching
}

bool I2CBus::GetSDA() const
{
  switch (state)
  {
  case State::Inactive:
  case State::Activating:
  default:
    return true;  // passive pullup (or NACK)

  case State::SetI2CAddress:
  case State::WriteDeviceAddress:
  case State::WriteToDevice:
    if (bit_counter < 8)
      return true;  // passive pullup
    else
      return false;  // ACK (if we need to NACK, we set the state to inactive)

  case State::ReadFromDevice:
    if (bit_counter < 8)
      return ((current_byte << bit_counter) & 0x80) != 0;
    else
      return true;  // passive pullup, receiver needs to ACK or NACK
  }
}

u32 WiiIPC::GetGPIOOut()
{
  // In the direction field, a '1' bit for a pin indicates that it will behave as an output (drive),
  // while a '0' bit tristates the pin and it becomes a high-impedance input.
  // In practice this means that (at least for the AVE I²C pins) a 1 is output when the pin is an
  // input. (RVLoader depends on this.)
  // https://github.com/Aurelio92/RVLoader/blob/75732f248019f589deb1109bba7b5323a8afaadf/source/i2c.c#L101-L109
  return (m_gpio_out.m_hex | ~(m_gpio_dir.m_hex)) & (ReadGPIOIn(m_system) | m_gpio_dir.m_hex);
}

void WiiIPC::GPIOOutChanged(u32 old_value_hex)
{
  const Common::Flags<GPIO> old_value(old_value_hex);
  const Common::Flags<GPIO> new_value(GetGPIOOut());

  if (new_value[GPIO::DO_EJECT])
  {
    INFO_LOG_FMT(WII_IPC, "Ejecting disc due to GPIO write");
    m_system.GetDVDInterface().EjectDisc(Core::CPUThreadGuard{m_system}, DVD::EjectCause::Software);
  }

  // I²C logic for the audio/video encoder (AVE)
  i2c_state.Update(m_system, old_value[GPIO::AVE_SCL], new_value[GPIO::AVE_SCL],
                   old_value[GPIO::AVE_SDA], new_value[GPIO::AVE_SDA]);

  // SENSOR_BAR is checked by WiimoteEmu::CameraLogic
  // TODO: SLOT_LED
}

void I2CBus::Start()
{
  if (state != State::Inactive)
    INFO_LOG_FMT(WII_IPC, "AVE: Re-start I2C");
  else
    INFO_LOG_FMT(WII_IPC, "AVE: Start I2C");

  if (bit_counter != 0)
    WARN_LOG_FMT(WII_IPC, "I2C: Start happened with a nonzero bit counter: {}", bit_counter);

  state = State::Activating;
  bit_counter = 0;
  current_byte = 0;
  i2c_address.reset();
  // Note: don't reset device_address, as it's re-used for reads
}

void I2CBus::Stop()
{
  INFO_LOG_FMT(WII_IPC, "AVE: Stop I2C");
  state = State::Inactive;
  bit_counter = 0;
  current_byte = 0;
  i2c_address.reset();
  device_address.reset();
}

bool I2CBus::WriteExpected() const
{
  // If we don't have an I²C address, it needs to be written (even if the address that is later
  // written is a read).
  // Otherwise, check the least significant bit; it being *clear* indicates a write.
  const bool is_write = !i2c_address.has_value() || ((i2c_address.value() & 1) == 0);
  // The device that is otherwise recieving instead transmits an acknowledge bit after each byte.
  const bool acknowledge_expected = (bit_counter == 8);

  return is_write ^ acknowledge_expected;
}

void I2CBus::Update(Core::System& system, const bool old_scl, const bool new_scl,
                    const bool old_sda, const bool new_sda)
{
  if (old_scl != new_scl && old_sda != new_sda)
  {
    ERROR_LOG_FMT(WII_IPC, "Both SCL and SDA changed at the same time: SCL {} -> {} SDA {} -> {}",
                  old_scl, new_scl, old_sda, new_sda);
    return;
  }

  if (old_scl == new_scl && old_sda == new_sda)
    return;  // Nothing changed

  if (old_scl && new_scl)
  {
    // Check for changes to SDA while the clock is high.
    if (old_sda && !new_sda)
    {
      // SDA falling edge (now pulled low) while SCL is high indicates I²C start
      Start();
    }
    else if (!old_sda && new_sda)
    {
      // SDA rising edge (now passive pullup) while SCL is high indicates I²C stop
      Stop();
    }
  }
  else if (state != State::Inactive)
  {
    if (!old_scl && new_scl)
    {
      SCLRisingEdge(new_sda);
    }
    else if (old_scl && !new_scl)
    {
      SCLFallingEdge(new_sda);
    }
  }
}

void I2CBus::SCLRisingEdge(const bool sda)
{
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} rising edge: {} (write expected: {})", bit_counter, state,
  //              sda, WriteExpected());
  // SCL rising edge indicates data clocking. For reads, we set up data at this point.
  // For writes, we instead process it on the falling edge, to better distinguish
  // the start/stop condition.
  if (state == State::ReadFromDevice && bit_counter == 0)
  {
    // Start of a read.
    ASSERT(device_address.has_value());  // Implied by the state transition in falling edge
    current_byte = reinterpret_cast<u8*>(&ave_state)[device_address.value()];
    INFO_LOG_FMT(WII_IPC, "AVE: Read from {:02x} ({}) -> {:02x}", device_address.value(),
                 GetAVERegisterName(device_address.value()), current_byte);
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

void I2CBus::SCLFallingEdge(const bool sda)
{
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} falling edge: {} (write expected: {})", bit_counter, state,
  //              sda, WriteExpected());
  // SCL falling edge is used to advance bit_counter/change states and process writes.
  if (state == State::SetI2CAddress || state == State::WriteDeviceAddress ||
      state == State::WriteToDevice)
  {
    if (bit_counter == 8)
    {
      // Acknowledge bit for *reads*.
      if (sda)
      {
        WARN_LOG_FMT(WII_IPC, "Read NACK'd");
        state = State::Inactive;
      }
    }
    else
    {
      current_byte <<= 1;
      if (sda)
        current_byte |= 1;

      if (bit_counter == 7)
      {
        INFO_LOG_FMT(WII_IPC, "AVE: Byte written: {:02x}", current_byte);
        // Write finished.
        if (state == State::SetI2CAddress)
        {
          if ((current_byte >> 1) != 0x70)
          {
            state = State::Inactive;  // NACK
            WARN_LOG_FMT(WII_IPC, "AVE: Unknown I2C address {:02x}", current_byte);
          }
          else
          {
            INFO_LOG_FMT(WII_IPC, "AVE: I2C address is {:02x}", current_byte);
          }
        }
        else if (state == State::WriteDeviceAddress)
        {
          device_address = current_byte;
          INFO_LOG_FMT(WII_IPC, "AVE: Device address is {:02x}", current_byte);
        }
        else
        {
          // Actual write
          ASSERT(state == State::WriteToDevice);
          ASSERT(device_address.has_value());  // implied by state transition
          const u8 old_ave_value = reinterpret_cast<u8*>(&ave_state)[device_address.value()];
          reinterpret_cast<u8*>(&ave_state)[device_address.value()] = current_byte;
          if (!ave_ever_logged[device_address.value()] || old_ave_value != current_byte)
          {
            INFO_LOG_FMT(WII_IPC, "AVE: Wrote {:02x} to {:02x} ({})", current_byte,
                         device_address.value(), GetAVERegisterName(device_address.value()));
            ave_ever_logged[device_address.value()] = true;
          }
          else
          {
            DEBUG_LOG_FMT(WII_IPC, "AVE: Wrote {:02x} to {:02x} ({})", current_byte,
                          device_address.value(), GetAVERegisterName(device_address.value()));
          }
          device_address = device_address.value() + 1;
        }
      }
    }
  }

  if (state == State::Activating)
  {
    // This is triggered after the start condition.
    state = State::SetI2CAddress;
    bit_counter = 0;
  }
  else if (state != State::Inactive)
  {
    if (bit_counter >= 8)
    {
      // Finished a byte and the acknowledge signal.
      bit_counter = 0;
      switch (state)
      {
      case State::SetI2CAddress:
        i2c_address = current_byte;
        // Note: i2c_address is known to correspond to a valid device
        if ((current_byte & 1) == 0)
        {
          state = State::WriteDeviceAddress;
          device_address.reset();
        }
        else
        {
          if (device_address.has_value())
          {
            state = State::ReadFromDevice;
          }
          else
          {
            state = State::Inactive;  // NACK - required for 8-bit internal addresses
            ERROR_LOG_FMT(WII_IPC, "AVE: Attempted to read device without having a read address!");
          }
        }
        break;
      case State::WriteDeviceAddress:
        state = State::WriteToDevice;
        break;
      }
    }
    else
    {
      bit_counter++;
    }
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

void WiiIPC::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | IPC_PPCMSG, MMIO::InvalidRead<u32>(), MMIO::DirectWrite<u32>(&m_ppc_msg));

  mmio->Register(base | IPC_PPCCTRL, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   auto& wii_ipc = system.GetWiiIPC();
                   return wii_ipc.m_ctrl.ppc();
                 }),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   wii_ipc.m_ctrl.ppc(val);
                   // The IPC interrupt is triggered when IY1/IY2 is set and
                   // Y1/Y2 is written to -- even when this results in clearing the bit.
                   if ((val >> 2 & 1 && wii_ipc.m_ctrl.IY1) || (val >> 1 & 1 && wii_ipc.m_ctrl.IY2))
                     wii_ipc.m_ppc_irq_flags |= INT_CAUSE_IPC_BROADWAY;
                   if (wii_ipc.m_ctrl.X1)
                     system.GetIOS()->EnqueueIPCRequest(wii_ipc.m_ppc_msg);
                   system.GetIOS()->UpdateIPC();
                   system.GetCoreTiming().ScheduleEvent(0, wii_ipc.m_event_type_update_interrupts,
                                                        0);
                 }));

  mmio->Register(base | IPC_ARMMSG, MMIO::DirectRead<u32>(&m_arm_msg), MMIO::InvalidWrite<u32>());

  mmio->Register(base | PPC_IRQFLAG, MMIO::InvalidRead<u32>(),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   wii_ipc.m_ppc_irq_flags &= ~val;
                   system.GetIOS()->UpdateIPC();
                   system.GetCoreTiming().ScheduleEvent(0, wii_ipc.m_event_type_update_interrupts,
                                                        0);
                 }));

  mmio->Register(base | PPC_IRQMASK, MMIO::InvalidRead<u32>(),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   wii_ipc.m_ppc_irq_masks = val;
                   if (wii_ipc.m_ppc_irq_masks & INT_CAUSE_IPC_BROADWAY)  // wtf?
                     wii_ipc.Reset();
                   system.GetIOS()->UpdateIPC();
                   system.GetCoreTiming().ScheduleEvent(0, wii_ipc.m_event_type_update_interrupts,
                                                        0);
                 }));

  mmio->Register(base | GPIOB_OUT, MMIO::DirectRead<u32>(&m_gpio_out.m_hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   const u32 old_out = wii_ipc.GetGPIOOut();
                   wii_ipc.m_gpio_out.m_hex =
                       (val & gpio_owner.m_hex) | (wii_ipc.m_gpio_out.m_hex & ~gpio_owner.m_hex);
                   wii_ipc.GPIOOutChanged(old_out);
                 }));
  mmio->Register(base | GPIOB_DIR, MMIO::DirectRead<u32>(&m_gpio_dir.m_hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   const u32 old_out = wii_ipc.GetGPIOOut();
                   wii_ipc.m_gpio_dir.m_hex =
                       (val & gpio_owner.m_hex) | (wii_ipc.m_gpio_dir.m_hex & ~gpio_owner.m_hex);
                   wii_ipc.GPIOOutChanged(old_out);
                 }));
  mmio->Register(base | GPIOB_IN, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   return ReadGPIOIn(system);
                 }),
                 MMIO::Nop<u32>());
  // Starlet GPIO registers, not normally accessible by PPC (but they can be depending on how
  // AHBPROT is set up).  We just always allow access, since some homebrew uses them.

  // Note from WiiBrew: When switching owners, copying of the data is not necessary. For example, if
  // pin 0 has certain configuration in the HW_GPIO registers, and that bit is then set in the
  // HW_GPIO_OWNER register, those settings will immediately be visible in the HW_GPIOB registers.
  // There is only one set of data registers, and the HW_GPIO_OWNER register just controls the
  // access that the HW_GPIOB registers have to that data.
  // Also: The HW_GPIO registers always have read access to all pins, but any writes (changes) must
  // go through the HW_GPIOB registers if the corresponding bit is set in the HW_GPIO_OWNER
  // register.
  mmio->Register(base | GPIO_OUT, MMIO::DirectRead<u32>(&m_gpio_out.m_hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   const u32 old_out = wii_ipc.GetGPIOOut();
                   wii_ipc.m_gpio_out.m_hex =
                       (val & ~gpio_owner.m_hex) | (wii_ipc.m_gpio_out.m_hex & gpio_owner.m_hex);
                   wii_ipc.GPIOOutChanged(old_out);
                 }));
  mmio->Register(base | GPIO_DIR, MMIO::DirectRead<u32>(&m_gpio_dir.m_hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& wii_ipc = system.GetWiiIPC();
                   const u32 old_out = wii_ipc.GetGPIOOut();
                   wii_ipc.m_gpio_dir.m_hex =
                       (val & gpio_owner.m_hex) | (wii_ipc.m_gpio_dir.m_hex & ~gpio_owner.m_hex);
                   wii_ipc.GPIOOutChanged(old_out);
                 }));
  mmio->Register(base | GPIO_IN, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   return ReadGPIOIn(system);
                 }),
                 MMIO::Nop<u32>());

  mmio->Register(base | RESETS, MMIO::DirectRead<u32>(&m_resets),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   // A reset occurs when the corresponding bit is cleared
                   auto& wii_ipc = system.GetWiiIPC();
                   const bool di_reset_triggered = (wii_ipc.m_resets & 0x400) && !(val & 0x400);
                   wii_ipc.m_resets = val;
                   if (di_reset_triggered)
                   {
                     // The GPIO *disables* spinning up the drive
                     const bool spinup = !wii_ipc.m_gpio_out[GPIO::DI_SPIN];
                     INFO_LOG_FMT(WII_IPC, "Resetting DI {} spinup", spinup ? "with" : "without");
                     system.GetDVDInterface().ResetDrive(spinup);
                   }
                 }));

  // Register some stubbed/unknown MMIOs required to make Wii games work.
  mmio->Register(base | VI1CFG, MMIO::InvalidRead<u32>(), MMIO::InvalidWrite<u32>());
  mmio->Register(base | VIDIM, MMIO::InvalidRead<u32>(), MMIO::InvalidWrite<u32>());
  mmio->Register(base | VISOLID, MMIO::InvalidRead<u32>(), MMIO::InvalidWrite<u32>());
  mmio->Register(base | COMPAT, MMIO::Constant<u32>(0), MMIO::Nop<u32>());
  mmio->Register(base | UNK_1CC, MMIO::Constant<u32>(0), MMIO::Nop<u32>());
  mmio->Register(base | UNK_1D0, MMIO::Constant<u32>(0), MMIO::Nop<u32>());
}

void WiiIPC::UpdateInterruptsCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  system.GetWiiIPC().UpdateInterrupts();
}

void WiiIPC::UpdateInterrupts()
{
  if ((m_ctrl.Y1 & m_ctrl.IY1) || (m_ctrl.Y2 & m_ctrl.IY2))
  {
    m_ppc_irq_flags |= INT_CAUSE_IPC_BROADWAY;
  }

  if ((m_ctrl.X1 & m_ctrl.IX1) || (m_ctrl.X2 & m_ctrl.IX2))
  {
    m_ppc_irq_flags |= INT_CAUSE_IPC_STARLET;
  }

  // Generate interrupt on PI if any of the devices behind starlet have an interrupt and mask is set
  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_WII_IPC,
                                                !!(m_ppc_irq_flags & m_ppc_irq_masks));
}

void WiiIPC::ClearX1()
{
  m_ctrl.X1 = 0;
}

void WiiIPC::GenerateAck(u32 address)
{
  m_ctrl.Y2 = 1;
  DEBUG_LOG_FMT(WII_IPC, "GenerateAck: {:08x} | {:08x} [R:{} A:{} E:{}]", m_ppc_msg, address,
                m_ctrl.Y1, m_ctrl.Y2, m_ctrl.X1);
  // Based on a hardware test, the IPC interrupt takes approximately 100 TB ticks to fire
  // after Y2 is seen in the control register.
  m_system.GetCoreTiming().ScheduleEvent(100_tbticks, m_event_type_update_interrupts);
}

void WiiIPC::GenerateReply(u32 address)
{
  m_arm_msg = address;
  m_ctrl.Y1 = 1;
  DEBUG_LOG_FMT(WII_IPC, "GenerateReply: {:08x} | {:08x} [R:{} A:{} E:{}]", m_ppc_msg, address,
                m_ctrl.Y1, m_ctrl.Y2, m_ctrl.X1);
  // Based on a hardware test, the IPC interrupt takes approximately 100 TB ticks to fire
  // after Y1 is seen in the control register.
  m_system.GetCoreTiming().ScheduleEvent(100_tbticks, m_event_type_update_interrupts);
}

bool WiiIPC::IsReady() const
{
  return ((m_ctrl.Y1 == 0) && (m_ctrl.Y2 == 0) &&
          ((m_ppc_irq_flags & INT_CAUSE_IPC_BROADWAY) == 0));
}
}  // namespace IOS

template <>
struct fmt::formatter<IOS::I2CBus::State> : EnumFormatter<IOS::I2CBus::State::ReadFromDevice>
{
  static constexpr array_type names = {"Inactive",        "Activating",
                                       "Set I2C Address", "Write Device Address",
                                       "Write To Device", "Read From Device"};
  constexpr formatter() : EnumFormatter(names) {}
};
