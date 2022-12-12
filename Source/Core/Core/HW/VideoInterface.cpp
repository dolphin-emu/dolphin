// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/VideoInterface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DiscIO/Enums.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoInterface
{
struct VideoInterfaceState::Data
{
  // Registers listed in order:
  UVIVerticalTimingRegister vertical_timing_register;
  UVIDisplayControlRegister display_control_register;
  UVIHorizontalTiming0 h_timing_0;
  UVIHorizontalTiming1 h_timing_1;
  UVIVBlankTimingRegister vblank_timing_odd;
  UVIVBlankTimingRegister vblank_timing_even;
  UVIBurstBlankingRegister burst_blanking_odd;
  UVIBurstBlankingRegister burst_blanking_even;
  UVIFBInfoRegister xfb_info_top;
  UVIFBInfoRegister xfb_info_bottom;
  UVIFBInfoRegister xfb_3d_info_top;  // Start making your stereoscopic demos! :p
  UVIFBInfoRegister xfb_3d_info_bottom;
  std::array<UVIInterruptRegister, 4> interrupt_register;
  std::array<UVILatchRegister, 2> latch_register;
  PictureConfigurationRegister picture_configuration;
  UVIHorizontalScaling horizontal_scaling;
  SVIFilterCoefTables filter_coef_tables;
  u32 unknown_aa_register = 0;  // ??? 0x00FF0000
  u16 clock = 0;                // 0: 27MHz, 1: 54MHz
  UVIDTVStatus dtv_status;
  UVIHorizontalStepping fb_width;  // Only correct when scaling is enabled?
  UVIBorderBlankRegister border_hblank;
  // 0xcc002076 - 0xcc00207f is full of 0x00FF: unknown
  // 0xcc002080 - 0xcc002100 even more unknown

  double target_refresh_rate = 0;
  u32 target_refresh_rate_numerator = 0;
  u32 target_refresh_rate_denominator = 1;

  u64 ticks_last_line_start;      // number of ticks when the current full scanline started
  u32 half_line_count;            // number of halflines that have occurred for this full frame
  u32 half_line_of_next_si_poll;  // halfline when next SI poll results should be available

  // below indexes are 0-based
  u32 even_field_first_hl;  // index first halfline of the even field
  u32 odd_field_first_hl;   // index first halfline of the odd field
  u32 even_field_last_hl;   // index last halfline of the even field
  u32 odd_field_last_hl;    // index last halfline of the odd field
};

VideoInterfaceState::VideoInterfaceState() : m_data(std::make_unique<Data>())
{
}

VideoInterfaceState::~VideoInterfaceState() = default;

static constexpr std::array<u32, 2> CLOCK_FREQUENCIES{{
    27000000,
    54000000,
}};

static constexpr u32 NUM_HALF_LINES_FOR_SI_POLL = (7 * 2) + 1;  // this is how long an SI poll takes

void DoState(PointerWrap& p)
{
  p.Do(Core::System::GetInstance().GetVideoInterfaceState().GetData());
}

// Executed after Init, before game boot
void Preset(bool _bNTSC)
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  // NOTE: Make sure all registers are set to the correct initial state. The
  //	variables are not going to start zeroed if another game has been run
  //	previously (and mutated everything).

  state.vertical_timing_register.EQU = 6;
  state.vertical_timing_register.ACV = 0;

  state.display_control_register.ENB = 1;
  state.display_control_register.RST = 0;
  state.display_control_register.NIN = 0;
  state.display_control_register.DLR = 0;
  state.display_control_register.LE0 = 0;
  state.display_control_register.LE1 = 0;
  state.display_control_register.FMT = _bNTSC ? 0 : 1;

  state.h_timing_0.HLW = 429;
  state.h_timing_0.HCE = 105;
  state.h_timing_0.HCS = 71;
  state.h_timing_1.HSY = 64;
  state.h_timing_1.HBE640 = 162;
  state.h_timing_1.HBS640 = 373;

  state.vblank_timing_odd.PRB = 502;
  state.vblank_timing_odd.PSB = 5;
  state.vblank_timing_even.PRB = 503;
  state.vblank_timing_even.PSB = 4;

  state.burst_blanking_odd.BS0 = 12;
  state.burst_blanking_odd.BE0 = 520;
  state.burst_blanking_odd.BS2 = 12;
  state.burst_blanking_odd.BE2 = 520;
  state.burst_blanking_even.BS0 = 13;
  state.burst_blanking_even.BE0 = 519;
  state.burst_blanking_even.BS2 = 13;
  state.burst_blanking_even.BE2 = 519;

  state.xfb_info_top.Hex = 0;
  state.xfb_info_bottom.Hex = 0;
  state.xfb_3d_info_top.Hex = 0;
  state.xfb_3d_info_bottom.Hex = 0;

  state.interrupt_register[0].HCT = 430;
  state.interrupt_register[0].VCT = 263;
  state.interrupt_register[0].IR_MASK = 1;
  state.interrupt_register[0].IR_INT = 0;
  state.interrupt_register[1].HCT = 1;
  state.interrupt_register[1].VCT = 1;
  state.interrupt_register[1].IR_MASK = 1;
  state.interrupt_register[1].IR_INT = 0;
  state.interrupt_register[2].Hex = 0;
  state.interrupt_register[3].Hex = 0;

  state.latch_register = {};

  state.picture_configuration.STD = 40;
  state.picture_configuration.WPL = 40;

  state.horizontal_scaling.Hex = 0;
  state.filter_coef_tables = {};
  state.unknown_aa_register = 0;

  DiscIO::Region region = SConfig::GetInstance().m_region;

  // 54MHz, capable of progressive scan
  state.clock = DiscIO::IsNTSC(region);

  // Say component cable is plugged
  state.dtv_status.component_plugged = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  state.dtv_status.ntsc_j = region == DiscIO::Region::NTSC_J;

  state.fb_width.Hex = 0;
  state.border_hblank.Hex = 0;

  state.ticks_last_line_start = 0;
  state.half_line_count = 0;
  state.half_line_of_next_si_poll = NUM_HALF_LINES_FOR_SI_POLL;  // first sampling starts at vsync

  UpdateParameters();
}

void Init()
{
  Preset(true);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  struct MappedVar
  {
    u32 addr;
    u16* ptr;
  };

  std::array<MappedVar, 46> directly_mapped_vars{{
      {VI_VERTICAL_TIMING, &state.vertical_timing_register.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &state.h_timing_0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &state.h_timing_0.Lo},
      {VI_HORIZONTAL_TIMING_1_HI, &state.h_timing_1.Hi},
      {VI_HORIZONTAL_TIMING_1_LO, &state.h_timing_1.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &state.vblank_timing_odd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &state.vblank_timing_odd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &state.vblank_timing_even.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &state.vblank_timing_even.Lo},
      {VI_BURST_BLANKING_ODD_HI, &state.burst_blanking_odd.Hi},
      {VI_BURST_BLANKING_ODD_LO, &state.burst_blanking_odd.Lo},
      {VI_BURST_BLANKING_EVEN_HI, &state.burst_blanking_even.Hi},
      {VI_BURST_BLANKING_EVEN_LO, &state.burst_blanking_even.Lo},
      {VI_FB_LEFT_TOP_LO, &state.xfb_info_top.Lo},
      {VI_FB_RIGHT_TOP_LO, &state.xfb_3d_info_top.Lo},
      {VI_FB_LEFT_BOTTOM_LO, &state.xfb_info_bottom.Lo},
      {VI_FB_RIGHT_BOTTOM_LO, &state.xfb_3d_info_bottom.Lo},
      {VI_PRERETRACE_LO, &state.interrupt_register[0].Lo},
      {VI_POSTRETRACE_LO, &state.interrupt_register[1].Lo},
      {VI_DISPLAY_INTERRUPT_2_LO, &state.interrupt_register[2].Lo},
      {VI_DISPLAY_INTERRUPT_3_LO, &state.interrupt_register[3].Lo},
      {VI_DISPLAY_LATCH_0_HI, &state.latch_register[0].Hi},
      {VI_DISPLAY_LATCH_0_LO, &state.latch_register[0].Lo},
      {VI_DISPLAY_LATCH_1_HI, &state.latch_register[1].Hi},
      {VI_DISPLAY_LATCH_1_LO, &state.latch_register[1].Lo},
      {VI_HSCALEW, &state.picture_configuration.Hex},
      {VI_HSCALER, &state.horizontal_scaling.Hex},
      {VI_FILTER_COEF_0_HI, &state.filter_coef_tables.Tables02[0].Hi},
      {VI_FILTER_COEF_0_LO, &state.filter_coef_tables.Tables02[0].Lo},
      {VI_FILTER_COEF_1_HI, &state.filter_coef_tables.Tables02[1].Hi},
      {VI_FILTER_COEF_1_LO, &state.filter_coef_tables.Tables02[1].Lo},
      {VI_FILTER_COEF_2_HI, &state.filter_coef_tables.Tables02[2].Hi},
      {VI_FILTER_COEF_2_LO, &state.filter_coef_tables.Tables02[2].Lo},
      {VI_FILTER_COEF_3_HI, &state.filter_coef_tables.Tables36[0].Hi},
      {VI_FILTER_COEF_3_LO, &state.filter_coef_tables.Tables36[0].Lo},
      {VI_FILTER_COEF_4_HI, &state.filter_coef_tables.Tables36[1].Hi},
      {VI_FILTER_COEF_4_LO, &state.filter_coef_tables.Tables36[1].Lo},
      {VI_FILTER_COEF_5_HI, &state.filter_coef_tables.Tables36[2].Hi},
      {VI_FILTER_COEF_5_LO, &state.filter_coef_tables.Tables36[2].Lo},
      {VI_FILTER_COEF_6_HI, &state.filter_coef_tables.Tables36[3].Hi},
      {VI_FILTER_COEF_6_LO, &state.filter_coef_tables.Tables36[3].Lo},
      {VI_CLOCK, &state.clock},
      {VI_DTV_STATUS, &state.dtv_status.Hex},
      {VI_FBWIDTH, &state.fb_width.Hex},
      {VI_BORDER_BLANK_END, &state.border_hblank.Lo},
      {VI_BORDER_BLANK_START, &state.border_hblank.Hi},
  }};

  // Declare all the boilerplate direct MMIOs.
  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::DirectWrite<u16>(mapped_var.ptr));
  }

  std::array<MappedVar, 8> update_params_on_read_vars{{
      {VI_VERTICAL_TIMING, &state.vertical_timing_register.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &state.h_timing_0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &state.h_timing_0.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &state.vblank_timing_odd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &state.vblank_timing_odd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &state.vblank_timing_even.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &state.vblank_timing_even.Lo},
      {VI_CLOCK, &state.clock},
  }};

  // Declare all the MMIOs that update timing params.
  for (auto& mapped_var : update_params_on_read_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::ComplexWrite<u16>([mapped_var](Core::System&, u32, u16 val) {
                     *mapped_var.ptr = val;
                     UpdateParameters();
                   }));
  }

  // XFB related MMIOs that require special handling on writes.
  mmio->Register(base | VI_FB_LEFT_TOP_HI, MMIO::DirectRead<u16>(&state.xfb_info_top.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.xfb_info_top.Hi = val;
                   if (state.xfb_info_top.CLRPOFF)
                     state.xfb_info_top.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_LEFT_BOTTOM_HI, MMIO::DirectRead<u16>(&state.xfb_info_bottom.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.xfb_info_bottom.Hi = val;
                   if (state.xfb_info_bottom.CLRPOFF)
                     state.xfb_info_bottom.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_TOP_HI, MMIO::DirectRead<u16>(&state.xfb_3d_info_top.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.xfb_3d_info_top.Hi = val;
                   if (state.xfb_3d_info_top.CLRPOFF)
                     state.xfb_3d_info_top.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_BOTTOM_HI, MMIO::DirectRead<u16>(&state.xfb_3d_info_bottom.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.xfb_3d_info_bottom.Hi = val;
                   if (state.xfb_3d_info_bottom.CLRPOFF)
                     state.xfb_3d_info_bottom.POFF = 0;
                 }));

  // MMIOs with unimplemented writes that trigger warnings.
  mmio->Register(
      base | VI_VERTICAL_BEAM_POSITION, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
        auto& state = system.GetVideoInterfaceState().GetData();
        return 1 + (state.half_line_count) / 2;
      }),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        WARN_LOG_FMT(
            VIDEOINTERFACE,
            "Changing vertical beam position to {:#06x} - not documented or implemented yet", val);
      }));
  mmio->Register(
      base | VI_HORIZONTAL_BEAM_POSITION, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
        auto& state = system.GetVideoInterfaceState().GetData();
        u16 value = static_cast<u16>(
            1 + state.h_timing_0.HLW *
                    (system.GetCoreTiming().GetTicks() - state.ticks_last_line_start) /
                    (GetTicksPerHalfLine()));
        return std::clamp<u16>(value, 1, state.h_timing_0.HLW * 2);
      }),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        WARN_LOG_FMT(
            VIDEOINTERFACE,
            "Changing horizontal beam position to {:#06x} - not documented or implemented yet",
            val);
      }));

  // The following MMIOs are interrupts related and update interrupt status
  // on writes.
  mmio->Register(base | VI_PRERETRACE_HI, MMIO::DirectRead<u16>(&state.interrupt_register[0].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.interrupt_register[0].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_POSTRETRACE_HI, MMIO::DirectRead<u16>(&state.interrupt_register[1].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.interrupt_register[1].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_2_HI,
                 MMIO::DirectRead<u16>(&state.interrupt_register[2].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.interrupt_register[2].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_3_HI,
                 MMIO::DirectRead<u16>(&state.interrupt_register[3].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.interrupt_register[3].Hi = val;
                   UpdateInterrupts();
                 }));

  // Unknown anti-aliasing related MMIO register: puts a warning on log and
  // needs to shift/mask when reading/writing.
  mmio->Register(base | VI_UNK_AA_REG_HI, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   return state.unknown_aa_register >> 16;
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.unknown_aa_register =
                       (state.unknown_aa_register & 0x0000FFFF) | ((u32)val << 16);
                   WARN_LOG_FMT(VIDEOINTERFACE, "Writing to the unknown AA register (hi)");
                 }));
  mmio->Register(base | VI_UNK_AA_REG_LO, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   return state.unknown_aa_register & 0xFFFF;
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();
                   state.unknown_aa_register = (state.unknown_aa_register & 0xFFFF0000) | val;
                   WARN_LOG_FMT(VIDEOINTERFACE, "Writing to the unknown AA register (lo)");
                 }));

  // Control register writes only updates some select bits, and additional
  // processing needs to be done if a reset is requested.
  mmio->Register(base | VI_CONTROL_REGISTER,
                 MMIO::DirectRead<u16>(&state.display_control_register.Hex),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& state = system.GetVideoInterfaceState().GetData();

                   UVIDisplayControlRegister tmpConfig(val);
                   state.display_control_register.ENB = tmpConfig.ENB;
                   state.display_control_register.NIN = tmpConfig.NIN;
                   state.display_control_register.DLR = tmpConfig.DLR;
                   state.display_control_register.LE0 = tmpConfig.LE0;
                   state.display_control_register.LE1 = tmpConfig.LE1;
                   state.display_control_register.FMT = tmpConfig.FMT;

                   if (tmpConfig.RST)
                   {
                     // shuffle2 clear all data, reset to default vals, and enter idle mode
                     state.display_control_register.RST = 0;
                     state.interrupt_register = {};
                     UpdateInterrupts();
                   }

                   UpdateParameters();
                 }));

  // Map 8 bit reads (not writes) to 16 bit reads.
  for (u32 i = 0; i < 0x1000; i += 2)
  {
    mmio->Register(base | i, MMIO::ReadToLarger<u8>(mmio, base | i, 8), MMIO::InvalidWrite<u8>());
    mmio->Register(base | (i + 1), MMIO::ReadToLarger<u8>(mmio, base | i, 0),
                   MMIO::InvalidWrite<u8>());
  }

  // Map 32 bit reads and writes to 16 bit reads and writes.
  for (u32 i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
                   MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2)));
  }
}

void UpdateInterrupts()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  if ((state.interrupt_register[0].IR_INT && state.interrupt_register[0].IR_MASK) ||
      (state.interrupt_register[1].IR_INT && state.interrupt_register[1].IR_MASK) ||
      (state.interrupt_register[2].IR_INT && state.interrupt_register[2].IR_MASK) ||
      (state.interrupt_register[3].IR_INT && state.interrupt_register[3].IR_MASK))
  {
    ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_VI, true);
  }
  else
  {
    ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_VI, false);
  }
}

u32 GetXFBAddressTop()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  if (state.xfb_info_top.POFF)
    return state.xfb_info_top.FBB << 5;
  else
    return state.xfb_info_top.FBB;
}

u32 GetXFBAddressBottom()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  // POFF for XFB bottom is connected to POFF for XFB top
  if (state.xfb_info_top.POFF)
    return state.xfb_info_bottom.FBB << 5;
  else
    return state.xfb_info_bottom.FBB;
}

static u32 GetHalfLinesPerEvenField()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return (3 * state.vertical_timing_register.EQU + state.vblank_timing_even.PRB +
          2 * state.vertical_timing_register.ACV + state.vblank_timing_even.PSB);
}

static u32 GetHalfLinesPerOddField()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return (3 * state.vertical_timing_register.EQU + state.vblank_timing_odd.PRB +
          2 * state.vertical_timing_register.ACV + state.vblank_timing_odd.PSB);
}

static u32 GetTicksPerEvenField()
{
  return GetTicksPerHalfLine() * GetHalfLinesPerEvenField();
}

static u32 GetTicksPerOddField()
{
  return GetTicksPerHalfLine() * GetHalfLinesPerOddField();
}

// Get the aspect ratio of VI's active area.
float GetAspectRatio()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  // The picture of a PAL/NTSC TV signal is defined to have a 4:3 aspect ratio,
  // but it's only 4:3 if the picture fill the entire active area.
  // All games configure VideoInterface to add padding in both the horizontal and vertical
  // directions and most games also do a slight horizontal scale.
  // This means that XFB never fills the entire active area and is therefor almost never 4:3

  // To work out the correct aspect ratio of the XFB, we need to know how VideoInterface's
  // currently configured active area compares to the active area of a stock PAL or NTSC
  // signal (which would be 4:3)

  // This function only deals with standard aspect ratios. For widescreen aspect ratios,
  // multiply the result by 1.33333..

  // 1. Get our active area in BT.601 samples (more or less pixels)
  int active_lines = state.vertical_timing_register.ACV;
  int active_width_samples =
      (state.h_timing_0.HLW + state.h_timing_1.HBS640 - state.h_timing_1.HBE640);

  // 2. TVs are analog and don't have pixels. So we convert to seconds.
  float tick_length = (1.0f / SystemTimers::GetTicksPerSecond());
  float vertical_period = tick_length * GetTicksPerField();
  float horizontal_period = tick_length * GetTicksPerHalfLine() * 2;
  float vertical_active_area = active_lines * horizontal_period;
  float horizontal_active_area = tick_length * GetTicksPerSample() * active_width_samples;

  // We are approximating the horizontal/vertical flyback transformers that control the
  // position of the electron beam on the screen. Our flyback transformers create a
  // perfect Sawtooth wave, with a smooth rise and a fall that takes zero time.
  // For more accurate emulation of video signals out of the 525 or 625 line standards,
  // it might be necessary to emulate a less precise flyback transformer with more flaws.
  // But those modes aren't officially supported by TVs anyway and could behave differently
  // on different TVs.

  // 3. Calculate the ratio of active time to total time for VI's active area
  float vertical_active_ratio = vertical_active_area / vertical_period;
  float horizontal_active_ratio = horizontal_active_area / horizontal_period;

  // 4. And then scale the ratios to typical PAL/NTSC signals.
  //    NOTE: With the exception of selecting between PAL-M and NTSC color encoding on Brazilian
  //          GameCubes, the FMT field doesn't actually do anything on real hardware. But
  //          Nintendo's SDK always sets it appropriately to match the number of lines.
  if (state.display_control_register.FMT == 1)  // 625 line TV (PAL)
  {
    // PAL defines the horizontal active area as 52us of the 64us line.
    // BT.470-6 defines the blanking period as 12.0us +0.0 -0.3 [table on page 5]
    horizontal_active_ratio *= 64.0f / 52.0f;
    // PAL defines the vertical active area as 576 of 625 lines.
    vertical_active_ratio *= 625.0f / 576.0f;
    // TODO: Should PAL60 games go through the 625 or 525 line codepath?
    //       The resulting aspect ratio is close, but not identical.
  }
  else  // 525 line TV (NTSC or PAL-M)
  {
    // The NTSC standard doesn't define it's active area very well.
    // The line is 63.55555..us long, which is derived from 1.001 / (30 * 525)
    // but the blanking area is defined with a large amount of slack in the SMPTE 170M-2004
    // standard, 10.7us +0.3 -0.2 [derived from table on page 9]
    // The BT.470-6 standard provides a different number of 10.9us +/- 0.2 [table on page 5]
    // This results in an active area between 52.5555us and 53.05555us
    // Lots of different numbers float around the Internet including:
    //   * 52.655555.. us --
    //   http://web.archive.org/web/20140218044518/http://lipas.uwasa.fi/~f76998/video/conversion/
    //   * 52.66 us -- http://www.ni.com/white-paper/4750/en/
    //   * 52.6 us -- http://web.mit.edu/6.111/www/f2008/handouts/L12.pdf
    //
    // None of these website provide primary sources for their numbers, back in the days of
    // analog, TV signal timings were not that precise to start with and it never got standardized
    // during the move to digital.
    // We are just going to use 52.655555.. as most other numbers on the Internet appear to be a
    // simplification of it. 53.655555.. is a blanking period of 10.9us, matching the BT.470-6
    // standard
    // and within tolerance of the SMPTE 170M-2004 standard.
    horizontal_active_ratio *= 63.555555f / 52.655555f;
    // Even 486 active lines isn't completely agreed upon.
    // Depending on how you count the two half lines you could get 485 or 484
    vertical_active_ratio *= 525.0f / 486.0f;
  }

  // 5. Calculate the final ratio and scale to 4:3
  float ratio = horizontal_active_ratio / vertical_active_ratio;
  bool running_fifo_log = FifoPlayer::GetInstance().IsRunningWithFakeVideoInterfaceUpdates();
  if (std::isnormal(ratio) &&      // Check we have a sane ratio without any infs/nans/zeros
      !running_fifo_log)           // we don't know the correct ratio for fifos
    return ratio * (4.0f / 3.0f);  // Scale to 4:3
  else
    return (4.0f / 3.0f);  // VI isn't initialized correctly, just return 4:3 instead
}

// This function updates:
// a) the scanlines that are considered the 'active region' of each field
// b) the equivalent refresh rate for the current timing configuration
//
// Each pair of fields is laid out like:
// [typical values are for NTSC interlaced]
//
// <---------- one scanline width ---------->
// EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
// ... lines omitted, 9 total E scanlines
// ... is typical
// EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
// RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR
// ... lines omitted, 12 total R scanlines
// ... is typical
// RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR
// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
// ... lines omitted, 240 total A scanlines
// ... is typical
// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
// SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS
// SSSSSSSSSSSSSSSSSSSSSeeeeeeeeeeeeeeeeeeeee
// eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
// ... lines omitted, 9 total e scanlines
// ... is typical
// eeeeeeeeeeeeeeeeeeeeerrrrrrrrrrrrrrrrrrrrr
// rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr
// ... lines omitted, 12.5 total r scanlines
// ... is typical
// rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr
// aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
// ... line omitted, 240 total a scanlines
// ... is typical
// aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
// ssssssssssssssssssssssssssssssssssssssssss
//
// Uppercase is field 1, lowercase is field 2
// E,e = pre-equ/vert-sync/post-equ
//     = (vertical_timing_register.EQU*3) half-scanlines
// R,r = preblanking
//     = (vblank_timing_x.PRB) half-scanlines
// A,a = active lines
//     = (vertical_timing_register.ACV*2) half-scanlines
// S,s = postblanking
//     = (vblank_timing_x.PSB) half-scanlines
//
// NB: for double-strike modes, the second field
//     does not get offset by half a scanline
//
// Some example video line layouts, based on values from titles:
// LXXX = Video line XXX, 0-based; hlYYYY = Video halfline YYYY, 0-based
// PAL
// EQU = 5
// ACV = 287
// OddPRB = 35
// OddPSB = 1
// EvenPRB = 36
// EvenPSB = 0
// L000 [ EQU | EQU ] [hl0000:hl0001]
// L001 [ EQU | EQU ] [hl0002:hl0003]
// ...
// L005 [ EQU | EQU ] [hl0010:hl0011]
// L006 [ EQU | EQU ] [hl0012:hl0013]
// L007 [ EQU | oPR ] [hl0014:hl0015]
// L008 [ oPR | oPR ] [hl0016:hl0017]
// L009 [ oPR | oPR ] [hl0018:hl0019]
// ...
// L023 [ oPR | oPR ] [hl0046:hl0047]
// L024 [ oPR | oPR ] [hl0048:hl0049]
// L025 [ ACV | ACV ] [hl0050:hl0051]
// L026 [ ACV | ACV ] [hl0052:hl0053]
// ...
// L310 [ ACV | ACV ] [hl0620:hl0621]
// L311 [ ACV | ACV ] [hl0622:hl0623]
// L312 [ oPS | EQU ] [hl0624:hl0625]
// L313 [ EQU | EQU ] [hl0626:hl0627]
// L314 [ EQU | EQU ] [hl0628:hl0629]
// ...
// L318 [ EQU | EQU ] [hl0636:hl0637]
// L319 [ EQU | EQU ] [hl0638:hl0639]
// L320 [ ePR | ePR ] [hl0640:hl0641]
// L321 [ ePR | ePR ] [hl0642:hl0643]
// ...
// L336 [ ePR | ePR ] [hl0672:hl0673]
// L337 [ ePR | ePR ] [hl0674:hl0675]
// L338 [ ACV | ACV ] [hl0676:hl0677]
// L339 [ ACV | ACV ] [hl0678:hl0679]
// ...
// L623 [ ACV | ACV ] [hl1246:hl1247]
// L624 [ ACV | ACV ] [hl1248:hl1249]
// (no ePS)
//
// NTSC
// EQU=6
// ACV=240
// OddPRB=24
// OddPSB=3
// EvenPRB=25
// EvenPSB=2
// L000 [ EQU | EQU ] [hl0000:hl0001]
// L001 [ EQU | EQU ] [hl0002:hl0003]
// ...
// L007 [ EQU | EQU ] [hl0014:hl0015]
// L008 [ EQU | EQU ] [hl0016:hl0017]
// L009 [ oPR | oPR ] [hl0018:hl0019]
// L010 [ oPR | oPR ] [hl0020:hl0021]
// ...
// L019 [ oPR | oPR ] [hl0038:hl0039]
// L020 [ oPR | oPR ] [hl0040:hl0041]
// L021 [ ACV | ACV ] [hl0042:hl0043]
// L022 [ ACV | ACV ] [hl0044:hl0045]
// ...
// L259 [ ACV | ACV ] [hl0518:hl0519]
// L260 [ ACV | ACV ] [hl0520:hl0521]
// L261 [ oPS | oPS ] [hl0522:hl0523]
// L262 [ oPS | EQU ] [hl0524:hl0525]
// L263 [ EQU | EQU ] [hl0526:hl0527]
// ...
// L270 [ EQU | EQU ] [hl0540:hl0541]
// L271 [ EQU | ePR ] [hl0542:hl0543]
// L272 [ ePR | ePR ] [hl0544:hl0545]
// L273 [ ePR | ePR ] [hl0546:hl0547]
// ...
// L282 [ ePR | ePR ] [hl0564:hl0565]
// L283 [ ePR | ePR ] [hl0566:hl0567]
// L284 [ ACV | ACV ] [hl0568:hl0569]
// L285 [ ACV | ACV ] [hl0570:hl0571]
// ...
// L522 [ ACV | ACV ] [hl1044:hl1045]
// L523 [ ACV | ACV ] [hl1046:hl1047]
// L524 [ ePS | ePS ] [hl1048:hl1049]

void UpdateParameters()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  u32 equ_hl = 3 * state.vertical_timing_register.EQU;
  u32 acv_hl = 2 * state.vertical_timing_register.ACV;
  state.odd_field_first_hl = equ_hl + state.vblank_timing_odd.PRB;
  state.odd_field_last_hl = state.odd_field_first_hl + acv_hl - 1;

  state.even_field_first_hl = equ_hl + state.vblank_timing_even.PRB + GetHalfLinesPerOddField();
  state.even_field_last_hl = state.even_field_first_hl + acv_hl - 1;

  state.target_refresh_rate_numerator = SystemTimers::GetTicksPerSecond() * 2;
  state.target_refresh_rate_denominator = GetTicksPerEvenField() + GetTicksPerOddField();
  state.target_refresh_rate = static_cast<double>(state.target_refresh_rate_numerator) /
                              state.target_refresh_rate_denominator;
}

double GetTargetRefreshRate()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return state.target_refresh_rate;
}

u32 GetTargetRefreshRateNumerator()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return state.target_refresh_rate_numerator;
}

u32 GetTargetRefreshRateDenominator()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return state.target_refresh_rate_denominator;
}

u32 GetTicksPerSample()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return 2 * SystemTimers::GetTicksPerSecond() / CLOCK_FREQUENCIES[state.clock];
}

u32 GetTicksPerHalfLine()
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();
  return GetTicksPerSample() * state.h_timing_0.HLW;
}

u32 GetTicksPerField()
{
  return GetTicksPerEvenField();
}

static void LogField(FieldType field, u32 xfb_address)
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  static constexpr std::array<const char*, 2> field_type_names{{"Odd", "Even"}};

  const std::array<const UVIVBlankTimingRegister*, 2> vert_timing{{
      &state.vblank_timing_odd,
      &state.vblank_timing_even,
  }};

  const auto field_index = static_cast<size_t>(field);

  DEBUG_LOG_FMT(VIDEOINTERFACE,
                "(VI->BeginField): Address: {:08X} | WPL {} | STD {} | EQ {} | PRB {} | "
                "ACV {} | PSB {} | Field {}",
                xfb_address, state.picture_configuration.WPL, state.picture_configuration.STD,
                state.vertical_timing_register.EQU, vert_timing[field_index]->PRB,
                state.vertical_timing_register.ACV, vert_timing[field_index]->PSB,
                field_type_names[field_index]);

  DEBUG_LOG_FMT(VIDEOINTERFACE, "HorizScaling: {:04x} | fbwidth {} | {} | {}",
                state.horizontal_scaling.Hex, state.fb_width.Hex, GetTicksPerEvenField(),
                GetTicksPerOddField());
}

static void OutputField(FieldType field, u64 ticks)
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  // Could we fit a second line of data in the stride?
  // (Datel's Wii FreeLoaders are the only titles known to set WPL to 0)
  bool potentially_interlaced_xfb =
      state.picture_configuration.WPL != 0 &&
      ((state.picture_configuration.STD / state.picture_configuration.WPL) == 2);
  // Are there an odd number of half-lines per field (definition of interlaced video)
  bool interlaced_video_mode = (GetHalfLinesPerEvenField() & 1) == 1;

  u32 fbStride = state.picture_configuration.STD * 16;
  u32 fbWidth = state.picture_configuration.WPL * 16;
  u32 fbHeight = state.vertical_timing_register.ACV;

  u32 xfbAddr;

  if (field == FieldType::Even)
  {
    xfbAddr = GetXFBAddressBottom();
  }
  else
  {
    xfbAddr = GetXFBAddressTop();
  }

  // Multiply the stride by 2 to get the byte offset for each subsequent line.
  fbStride *= 2;

  if (potentially_interlaced_xfb && interlaced_video_mode && g_ActiveConfig.bForceProgressive)
  {
    // Strictly speaking, in interlaced mode, we're only supposed to read
    // half of the lines of the XFB, and use that to display a field; the
    // other lines are unspecified junk.  However, in practice, we can
    // almost always double the vertical resolution of the output by
    // forcing progressive output: there's usually useful data in the
    // other field.  One notable exception: the title screen teaser
    // videos in Metroid Prime don't render correctly using this hack.
    fbStride /= 2;
    fbHeight *= 2;

    // PRB for the different fields should only ever differ by 1 in
    // interlaced mode, and which is less determines which field
    // has the first line. For the field with the second line, we
    // offset the xfb by (-stride_of_one_line) to get the start
    // address of the full xfb.
    if (field == FieldType::Odd &&
        state.vblank_timing_odd.PRB == state.vblank_timing_even.PRB + 1 && xfbAddr)
    {
      xfbAddr -= fbStride;
    }

    if (field == FieldType::Even &&
        state.vblank_timing_odd.PRB == state.vblank_timing_even.PRB - 1 && xfbAddr)
    {
      xfbAddr -= fbStride;
    }
  }

  LogField(field, xfbAddr);

  // Outputting the entire frame using a single set of VI register values isn't accurate, as games
  // can change the register values during scanout. To correctly emulate the scanout process, we
  // would need to collate all changes to the VI registers during scanout.
  if (xfbAddr)
    g_video_backend->Video_OutputXFB(xfbAddr, fbWidth, fbStride, fbHeight, ticks);
}

static void BeginField(FieldType field, u64 ticks)
{
  // Outputting the frame at the beginning of scanout reduces latency. This assumes the game isn't
  // going to change the VI registers while a frame is scanning out.
  if (Config::Get(Config::GFX_HACK_EARLY_XFB_OUTPUT))
    OutputField(field, ticks);
}

static void EndField(FieldType field, u64 ticks)
{
  // If the game does change VI registers while a frame is scanning out, we can defer output
  // until the end so the last register values are used. This still isn't accurate, but it does
  // produce more acceptable results in some problematic cases.
  // Currently, this is only known to be necessary to eliminate flickering in WWE Crush Hour.
  if (!Config::Get(Config::GFX_HACK_EARLY_XFB_OUTPUT))
    OutputField(field, ticks);

  Core::VideoThrottle();
  Core::OnFrameEnd();
}

// Purpose: Send VI interrupt when triggered
// Run when: When a frame is scanned (progressive/interlace)
void Update(u64 ticks)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetVideoInterfaceState().GetData();

  // Movie's frame counter should be updated before actually rendering the frame,
  // in case frame counter display is enabled

  if (state.half_line_count == 0 || state.half_line_count == GetHalfLinesPerEvenField())
    Movie::FrameUpdate();

  // If this half-line is at some boundary of the "active video lines" in either field, we either
  // need to (a) send a request to the GPU thread to actually render the XFB, or (b) increment
  // the number of frames we've actually drawn

  if (state.half_line_count == state.even_field_first_hl)
  {
    BeginField(FieldType::Even, ticks);
  }
  else if (state.half_line_count == state.odd_field_first_hl)
  {
    BeginField(FieldType::Odd, ticks);
  }
  else if (state.half_line_count == state.even_field_last_hl)
  {
    EndField(FieldType::Even, ticks);
  }
  else if (state.half_line_count == state.odd_field_last_hl)
  {
    EndField(FieldType::Odd, ticks);
  }

  // If this half-line is at a field boundary, deal with frame stepping before potentially
  // dealing with SI polls, but after potentially sending a swap request to the GPU thread

  if (state.half_line_count == 0 || state.half_line_count == GetHalfLinesPerEvenField())
    Core::Callback_NewField();

  // If an SI poll is scheduled to happen on this half-line, do it!

  if (state.half_line_of_next_si_poll == state.half_line_count)
  {
    Core::UpdateInputGate(!Config::Get(Config::MAIN_INPUT_BACKGROUND_INPUT),
                          Config::Get(Config::MAIN_LOCK_CURSOR));
    SerialInterface::UpdateDevices();
    state.half_line_of_next_si_poll += 2 * SerialInterface::GetPollXLines();
  }

  // If this half-line is at the actual boundary of either field, schedule an SI poll to happen
  // some number of half-lines in the future

  if (state.half_line_count == 0)
  {
    state.half_line_of_next_si_poll = NUM_HALF_LINES_FOR_SI_POLL;  // first results start at vsync
  }
  if (state.half_line_count == GetHalfLinesPerEvenField())
  {
    state.half_line_of_next_si_poll = GetHalfLinesPerEvenField() + NUM_HALF_LINES_FOR_SI_POLL;
  }

  // Move to the next half-line and potentially roll-over the count to zero. If we've reached
  // the beginning of a new full-line, update the timer

  state.half_line_count++;
  if (state.half_line_count == GetHalfLinesPerEvenField() + GetHalfLinesPerOddField())
  {
    state.half_line_count = 0;
  }

  if (!(state.half_line_count & 1))
  {
    state.ticks_last_line_start = system.GetCoreTiming().GetTicks();
  }

  // Check if we need to assert IR_INT. Note that the granularity of our current horizontal
  // position is limited to half-lines.

  for (UVIInterruptRegister& reg : state.interrupt_register)
  {
    u32 target_halfline = (reg.HCT > state.h_timing_0.HLW) ? 1 : 0;
    if ((1 + (state.half_line_count) / 2 == reg.VCT) &&
        ((state.half_line_count & 1) == target_halfline))
    {
      reg.IR_INT = 1;
    }
  }

  UpdateInterrupts();
}

// Create a fake VI mode for a fifolog
void FakeVIUpdate(u32 xfb_address, u32 fb_width, u32 fb_stride, u32 fb_height)
{
  auto& state = Core::System::GetInstance().GetVideoInterfaceState().GetData();

  bool interlaced = fb_height > 480 / 2;
  if (interlaced)
  {
    fb_height = fb_height / 2;
    fb_stride = fb_stride * 2;
  }

  state.xfb_info_top.POFF = 1;
  state.xfb_info_bottom.POFF = 1;
  state.vertical_timing_register.ACV = fb_height;
  state.vertical_timing_register.EQU = 6;
  state.vblank_timing_odd.PRB = 502 - fb_height * 2;
  state.vblank_timing_odd.PSB = 5;
  state.vblank_timing_even.PRB = 503 - fb_height * 2;
  state.vblank_timing_even.PSB = 4;
  state.picture_configuration.WPL = fb_width / 16;
  state.picture_configuration.STD = (fb_stride / 2) / 16;

  UpdateParameters();

  u32 total_halflines = GetHalfLinesPerEvenField() + GetHalfLinesPerOddField();

  if ((state.half_line_count - state.even_field_first_hl) % total_halflines <
      (state.half_line_count - state.odd_field_first_hl) % total_halflines)
  {
    // Even/Bottom field is next.
    state.xfb_info_bottom.FBB = interlaced ? (xfb_address + fb_width * 2) >> 5 : xfb_address >> 5;
  }
  else
  {
    // Odd/Top field is next
    state.xfb_info_top.FBB = (xfb_address >> 5);
  }
}

}  // namespace VideoInterface
