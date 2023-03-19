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

#include "VideoCommon/PerformanceMetrics.h"

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
VideoInterfaceManager::VideoInterfaceManager(Core::System& system) : m_system(system)
{
}

VideoInterfaceManager::~VideoInterfaceManager() = default;

static constexpr std::array<u32, 2> CLOCK_FREQUENCIES{{
    27000000,
    54000000,
}};

static constexpr u32 NUM_HALF_LINES_FOR_SI_POLL = (7 * 2) + 1;  // this is how long an SI poll takes

void VideoInterfaceManager::DoState(PointerWrap& p)
{
  p.Do(m_vertical_timing_register);
  p.Do(m_display_control_register);
  p.Do(m_h_timing_0);
  p.Do(m_h_timing_1);
  p.Do(m_vblank_timing_odd);
  p.Do(m_vblank_timing_even);
  p.Do(m_burst_blanking_odd);
  p.Do(m_burst_blanking_even);
  p.Do(m_xfb_info_top);
  p.Do(m_xfb_info_bottom);
  p.Do(m_xfb_3d_info_top);
  p.Do(m_xfb_3d_info_bottom);
  p.DoArray(m_interrupt_register);
  p.DoArray(m_latch_register);
  p.Do(m_picture_configuration);
  p.Do(m_horizontal_scaling);
  p.DoArray(m_filter_coef_tables.Tables02);
  p.DoArray(m_filter_coef_tables.Tables36);
  p.Do(m_unknown_aa_register);
  p.Do(m_clock);
  p.Do(m_dtv_status);
  p.Do(m_fb_width);
  p.Do(m_border_hblank);
  p.Do(m_target_refresh_rate);
  p.Do(m_target_refresh_rate_numerator);
  p.Do(m_target_refresh_rate_denominator);
  p.Do(m_ticks_last_line_start);
  p.Do(m_half_line_count);
  p.Do(m_half_line_of_next_si_poll);
  p.Do(m_even_field_first_hl);
  p.Do(m_odd_field_first_hl);
  p.Do(m_even_field_last_hl);
  p.Do(m_odd_field_last_hl);
}

// Executed after Init, before game boot
void VideoInterfaceManager::Preset(bool _bNTSC)
{
  // NOTE: Make sure all registers are set to the correct initial state. The
  //	variables are not going to start zeroed if another game has been run
  //	previously (and mutated everything).

  m_vertical_timing_register.EQU = 6;
  m_vertical_timing_register.ACV = 0;

  m_display_control_register.ENB = 1;
  m_display_control_register.RST = 0;
  m_display_control_register.NIN = 0;
  m_display_control_register.DLR = 0;
  m_display_control_register.LE0 = 0;
  m_display_control_register.LE1 = 0;
  m_display_control_register.FMT = _bNTSC ? 0 : 1;

  m_h_timing_0.HLW = 429;
  m_h_timing_0.HCE = 105;
  m_h_timing_0.HCS = 71;
  m_h_timing_1.HSY = 64;
  m_h_timing_1.HBE640 = 162;
  m_h_timing_1.HBS640 = 373;

  m_vblank_timing_odd.PRB = 502;
  m_vblank_timing_odd.PSB = 5;
  m_vblank_timing_even.PRB = 503;
  m_vblank_timing_even.PSB = 4;

  m_burst_blanking_odd.BS0 = 12;
  m_burst_blanking_odd.BE0 = 520;
  m_burst_blanking_odd.BS2 = 12;
  m_burst_blanking_odd.BE2 = 520;
  m_burst_blanking_even.BS0 = 13;
  m_burst_blanking_even.BE0 = 519;
  m_burst_blanking_even.BS2 = 13;
  m_burst_blanking_even.BE2 = 519;

  m_xfb_info_top.Hex = 0;
  m_xfb_info_bottom.Hex = 0;
  m_xfb_3d_info_top.Hex = 0;
  m_xfb_3d_info_bottom.Hex = 0;

  m_interrupt_register[0].HCT = 430;
  m_interrupt_register[0].VCT = 263;
  m_interrupt_register[0].IR_MASK = 1;
  m_interrupt_register[0].IR_INT = 0;
  m_interrupt_register[1].HCT = 1;
  m_interrupt_register[1].VCT = 1;
  m_interrupt_register[1].IR_MASK = 1;
  m_interrupt_register[1].IR_INT = 0;
  m_interrupt_register[2].Hex = 0;
  m_interrupt_register[3].Hex = 0;

  m_latch_register = {};

  m_picture_configuration.STD = 40;
  m_picture_configuration.WPL = 40;

  m_horizontal_scaling.Hex = 0;
  m_filter_coef_tables = {};
  m_unknown_aa_register = 0;

  DiscIO::Region region = SConfig::GetInstance().m_region;

  // 54MHz, capable of progressive scan
  m_clock = DiscIO::IsNTSC(region);

  // Say component cable is plugged
  m_dtv_status.component_plugged = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  m_dtv_status.ntsc_j = region == DiscIO::Region::NTSC_J;

  m_fb_width.Hex = 0;
  m_border_hblank.Hex = 0;

  m_ticks_last_line_start = 0;
  m_half_line_count = 0;
  m_half_line_of_next_si_poll = NUM_HALF_LINES_FOR_SI_POLL;  // first sampling starts at vsync

  UpdateParameters();
}

void VideoInterfaceManager::Init()
{
  Preset(true);
}

void VideoInterfaceManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  struct MappedVar
  {
    u32 addr;
    u16* ptr;
  };

  std::array<MappedVar, 46> directly_mapped_vars{{
      {VI_VERTICAL_TIMING, &m_vertical_timing_register.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &m_h_timing_0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &m_h_timing_0.Lo},
      {VI_HORIZONTAL_TIMING_1_HI, &m_h_timing_1.Hi},
      {VI_HORIZONTAL_TIMING_1_LO, &m_h_timing_1.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &m_vblank_timing_odd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &m_vblank_timing_odd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &m_vblank_timing_even.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &m_vblank_timing_even.Lo},
      {VI_BURST_BLANKING_ODD_HI, &m_burst_blanking_odd.Hi},
      {VI_BURST_BLANKING_ODD_LO, &m_burst_blanking_odd.Lo},
      {VI_BURST_BLANKING_EVEN_HI, &m_burst_blanking_even.Hi},
      {VI_BURST_BLANKING_EVEN_LO, &m_burst_blanking_even.Lo},
      {VI_FB_LEFT_TOP_LO, &m_xfb_info_top.Lo},
      {VI_FB_RIGHT_TOP_LO, &m_xfb_3d_info_top.Lo},
      {VI_FB_LEFT_BOTTOM_LO, &m_xfb_info_bottom.Lo},
      {VI_FB_RIGHT_BOTTOM_LO, &m_xfb_3d_info_bottom.Lo},
      {VI_PRERETRACE_LO, &m_interrupt_register[0].Lo},
      {VI_POSTRETRACE_LO, &m_interrupt_register[1].Lo},
      {VI_DISPLAY_INTERRUPT_2_LO, &m_interrupt_register[2].Lo},
      {VI_DISPLAY_INTERRUPT_3_LO, &m_interrupt_register[3].Lo},
      {VI_DISPLAY_LATCH_0_HI, &m_latch_register[0].Hi},
      {VI_DISPLAY_LATCH_0_LO, &m_latch_register[0].Lo},
      {VI_DISPLAY_LATCH_1_HI, &m_latch_register[1].Hi},
      {VI_DISPLAY_LATCH_1_LO, &m_latch_register[1].Lo},
      {VI_HSCALEW, &m_picture_configuration.Hex},
      {VI_HSCALER, &m_horizontal_scaling.Hex},
      {VI_FILTER_COEF_0_HI, &m_filter_coef_tables.Tables02[0].Hi},
      {VI_FILTER_COEF_0_LO, &m_filter_coef_tables.Tables02[0].Lo},
      {VI_FILTER_COEF_1_HI, &m_filter_coef_tables.Tables02[1].Hi},
      {VI_FILTER_COEF_1_LO, &m_filter_coef_tables.Tables02[1].Lo},
      {VI_FILTER_COEF_2_HI, &m_filter_coef_tables.Tables02[2].Hi},
      {VI_FILTER_COEF_2_LO, &m_filter_coef_tables.Tables02[2].Lo},
      {VI_FILTER_COEF_3_HI, &m_filter_coef_tables.Tables36[0].Hi},
      {VI_FILTER_COEF_3_LO, &m_filter_coef_tables.Tables36[0].Lo},
      {VI_FILTER_COEF_4_HI, &m_filter_coef_tables.Tables36[1].Hi},
      {VI_FILTER_COEF_4_LO, &m_filter_coef_tables.Tables36[1].Lo},
      {VI_FILTER_COEF_5_HI, &m_filter_coef_tables.Tables36[2].Hi},
      {VI_FILTER_COEF_5_LO, &m_filter_coef_tables.Tables36[2].Lo},
      {VI_FILTER_COEF_6_HI, &m_filter_coef_tables.Tables36[3].Hi},
      {VI_FILTER_COEF_6_LO, &m_filter_coef_tables.Tables36[3].Lo},
      {VI_CLOCK, &m_clock},
      {VI_DTV_STATUS, &m_dtv_status.Hex},
      {VI_FBWIDTH, &m_fb_width.Hex},
      {VI_BORDER_BLANK_END, &m_border_hblank.Lo},
      {VI_BORDER_BLANK_START, &m_border_hblank.Hi},
  }};

  // Declare all the boilerplate direct MMIOs.
  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::DirectWrite<u16>(mapped_var.ptr));
  }

  std::array<MappedVar, 8> update_params_on_read_vars{{
      {VI_VERTICAL_TIMING, &m_vertical_timing_register.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &m_h_timing_0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &m_h_timing_0.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &m_vblank_timing_odd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &m_vblank_timing_odd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &m_vblank_timing_even.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &m_vblank_timing_even.Lo},
      {VI_CLOCK, &m_clock},
  }};

  // Declare all the MMIOs that update timing params.
  for (auto& mapped_var : update_params_on_read_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::ComplexWrite<u16>([mapped_var](Core::System& system, u32, u16 val) {
                     *mapped_var.ptr = val;
                     system.GetVideoInterface().UpdateParameters();
                   }));
  }

  // XFB related MMIOs that require special handling on writes.
  mmio->Register(base | VI_FB_LEFT_TOP_HI, MMIO::DirectRead<u16>(&m_xfb_info_top.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_xfb_info_top.Hi = val;
                   if (vi.m_xfb_info_top.CLRPOFF)
                     vi.m_xfb_info_top.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_LEFT_BOTTOM_HI, MMIO::DirectRead<u16>(&m_xfb_info_bottom.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_xfb_info_bottom.Hi = val;
                   if (vi.m_xfb_info_bottom.CLRPOFF)
                     vi.m_xfb_info_bottom.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_TOP_HI, MMIO::DirectRead<u16>(&m_xfb_3d_info_top.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_xfb_3d_info_top.Hi = val;
                   if (vi.m_xfb_3d_info_top.CLRPOFF)
                     vi.m_xfb_3d_info_top.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_BOTTOM_HI, MMIO::DirectRead<u16>(&m_xfb_3d_info_bottom.Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_xfb_3d_info_bottom.Hi = val;
                   if (vi.m_xfb_3d_info_bottom.CLRPOFF)
                     vi.m_xfb_3d_info_bottom.POFF = 0;
                 }));

  // MMIOs with unimplemented writes that trigger warnings.
  mmio->Register(
      base | VI_VERTICAL_BEAM_POSITION, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
        auto& vi = system.GetVideoInterface();
        return 1 + (vi.m_half_line_count) / 2;
      }),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        WARN_LOG_FMT(
            VIDEOINTERFACE,
            "Changing vertical beam position to {:#06x} - not documented or implemented yet", val);
      }));
  mmio->Register(
      base | VI_HORIZONTAL_BEAM_POSITION, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
        auto& vi = system.GetVideoInterface();
        u16 value = static_cast<u16>(
            1 + vi.m_h_timing_0.HLW *
                    (system.GetCoreTiming().GetTicks() - vi.m_ticks_last_line_start) /
                    (vi.GetTicksPerHalfLine()));
        return std::clamp<u16>(value, 1, vi.m_h_timing_0.HLW * 2);
      }),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        WARN_LOG_FMT(
            VIDEOINTERFACE,
            "Changing horizontal beam position to {:#06x} - not documented or implemented yet",
            val);
      }));

  // The following MMIOs are interrupts related and update interrupt status
  // on writes.
  mmio->Register(base | VI_PRERETRACE_HI, MMIO::DirectRead<u16>(&m_interrupt_register[0].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_interrupt_register[0].Hi = val;
                   vi.UpdateInterrupts();
                 }));
  mmio->Register(base | VI_POSTRETRACE_HI, MMIO::DirectRead<u16>(&m_interrupt_register[1].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_interrupt_register[1].Hi = val;
                   vi.UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_2_HI,
                 MMIO::DirectRead<u16>(&m_interrupt_register[2].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_interrupt_register[2].Hi = val;
                   vi.UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_3_HI,
                 MMIO::DirectRead<u16>(&m_interrupt_register[3].Hi),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_interrupt_register[3].Hi = val;
                   vi.UpdateInterrupts();
                 }));

  // Unknown anti-aliasing related MMIO register: puts a warning on log and
  // needs to shift/mask when reading/writing.
  mmio->Register(base | VI_UNK_AA_REG_HI, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& vi = system.GetVideoInterface();
                   return vi.m_unknown_aa_register >> 16;
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_unknown_aa_register =
                       (vi.m_unknown_aa_register & 0x0000FFFF) | ((u32)val << 16);
                   WARN_LOG_FMT(VIDEOINTERFACE, "Writing to the unknown AA register (hi)");
                 }));
  mmio->Register(base | VI_UNK_AA_REG_LO, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& vi = system.GetVideoInterface();
                   return vi.m_unknown_aa_register & 0xFFFF;
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();
                   vi.m_unknown_aa_register = (vi.m_unknown_aa_register & 0xFFFF0000) | val;
                   WARN_LOG_FMT(VIDEOINTERFACE, "Writing to the unknown AA register (lo)");
                 }));

  // Control register writes only updates some select bits, and additional
  // processing needs to be done if a reset is requested.
  mmio->Register(base | VI_CONTROL_REGISTER, MMIO::DirectRead<u16>(&m_display_control_register.Hex),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& vi = system.GetVideoInterface();

                   UVIDisplayControlRegister tmpConfig(val);
                   vi.m_display_control_register.ENB = tmpConfig.ENB;
                   vi.m_display_control_register.NIN = tmpConfig.NIN;
                   vi.m_display_control_register.DLR = tmpConfig.DLR;
                   vi.m_display_control_register.LE0 = tmpConfig.LE0;
                   vi.m_display_control_register.LE1 = tmpConfig.LE1;
                   vi.m_display_control_register.FMT = tmpConfig.FMT;

                   if (tmpConfig.RST)
                   {
                     // shuffle2 clear all data, reset to default vals, and enter idle mode
                     vi.m_display_control_register.RST = 0;
                     vi.m_interrupt_register = {};
                     vi.UpdateInterrupts();
                   }

                   vi.UpdateParameters();
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

void VideoInterfaceManager::UpdateInterrupts()
{
  if ((m_interrupt_register[0].IR_INT && m_interrupt_register[0].IR_MASK) ||
      (m_interrupt_register[1].IR_INT && m_interrupt_register[1].IR_MASK) ||
      (m_interrupt_register[2].IR_INT && m_interrupt_register[2].IR_MASK) ||
      (m_interrupt_register[3].IR_INT && m_interrupt_register[3].IR_MASK))
  {
    m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_VI, true);
  }
  else
  {
    m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_VI, false);
  }
}

u32 VideoInterfaceManager::GetXFBAddressTop() const
{
  if (m_xfb_info_top.POFF)
    return m_xfb_info_top.FBB << 5;
  else
    return m_xfb_info_top.FBB;
}

u32 VideoInterfaceManager::GetXFBAddressBottom() const
{
  // POFF for XFB bottom is connected to POFF for XFB top
  if (m_xfb_info_top.POFF)
    return m_xfb_info_bottom.FBB << 5;
  else
    return m_xfb_info_bottom.FBB;
}

u32 VideoInterfaceManager::GetHalfLinesPerEvenField() const
{
  return (3 * m_vertical_timing_register.EQU + m_vblank_timing_even.PRB +
          2 * m_vertical_timing_register.ACV + m_vblank_timing_even.PSB);
}

u32 VideoInterfaceManager::GetHalfLinesPerOddField() const
{
  return (3 * m_vertical_timing_register.EQU + m_vblank_timing_odd.PRB +
          2 * m_vertical_timing_register.ACV + m_vblank_timing_odd.PSB);
}

u32 VideoInterfaceManager::GetTicksPerEvenField() const
{
  return GetTicksPerHalfLine() * GetHalfLinesPerEvenField();
}

u32 VideoInterfaceManager::GetTicksPerOddField() const
{
  return GetTicksPerHalfLine() * GetHalfLinesPerOddField();
}

// Get the aspect ratio of VI's active area.
float VideoInterfaceManager::GetAspectRatio() const
{
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
  int active_lines = m_vertical_timing_register.ACV;
  int active_width_samples = (m_h_timing_0.HLW + m_h_timing_1.HBS640 - m_h_timing_1.HBE640);

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
  if (m_display_control_register.FMT == 1)  // 625 line TV (PAL)
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

void VideoInterfaceManager::UpdateParameters()
{
  u32 equ_hl = 3 * m_vertical_timing_register.EQU;
  u32 acv_hl = 2 * m_vertical_timing_register.ACV;
  m_odd_field_first_hl = equ_hl + m_vblank_timing_odd.PRB;
  m_odd_field_last_hl = m_odd_field_first_hl + acv_hl - 1;

  m_even_field_first_hl = equ_hl + m_vblank_timing_even.PRB + GetHalfLinesPerOddField();
  m_even_field_last_hl = m_even_field_first_hl + acv_hl - 1;

  m_target_refresh_rate_numerator = SystemTimers::GetTicksPerSecond() * 2;
  m_target_refresh_rate_denominator = GetTicksPerEvenField() + GetTicksPerOddField();
  m_target_refresh_rate =
      static_cast<double>(m_target_refresh_rate_numerator) / m_target_refresh_rate_denominator;
}

double VideoInterfaceManager::GetTargetRefreshRate() const
{
  return m_target_refresh_rate;
}

u32 VideoInterfaceManager::GetTargetRefreshRateNumerator() const
{
  return m_target_refresh_rate_numerator;
}

u32 VideoInterfaceManager::GetTargetRefreshRateDenominator() const
{
  return m_target_refresh_rate_denominator;
}

u32 VideoInterfaceManager::GetTicksPerSample() const
{
  return 2 * SystemTimers::GetTicksPerSecond() / CLOCK_FREQUENCIES[m_clock];
}

u32 VideoInterfaceManager::GetTicksPerHalfLine() const
{
  return GetTicksPerSample() * m_h_timing_0.HLW;
}

u32 VideoInterfaceManager::GetTicksPerField() const
{
  return GetTicksPerEvenField();
}

void VideoInterfaceManager::LogField(FieldType field, u32 xfb_address) const
{
  static constexpr std::array<const char*, 2> field_type_names{{"Odd", "Even"}};

  const std::array<const UVIVBlankTimingRegister*, 2> vert_timing{{
      &m_vblank_timing_odd,
      &m_vblank_timing_even,
  }};

  const auto field_index = static_cast<size_t>(field);

  DEBUG_LOG_FMT(VIDEOINTERFACE,
                "(VI->BeginField): Address: {:08X} | WPL {} | STD {} | EQ {} | PRB {} | "
                "ACV {} | PSB {} | Field {}",
                xfb_address, m_picture_configuration.WPL, m_picture_configuration.STD,
                m_vertical_timing_register.EQU, vert_timing[field_index]->PRB,
                m_vertical_timing_register.ACV, vert_timing[field_index]->PSB,
                field_type_names[field_index]);

  DEBUG_LOG_FMT(VIDEOINTERFACE, "HorizScaling: {:04x} | fbwidth {} | {} | {}",
                m_horizontal_scaling.Hex, m_fb_width.Hex, GetTicksPerEvenField(),
                GetTicksPerOddField());
}

void VideoInterfaceManager::OutputField(FieldType field, u64 ticks)
{
  // Could we fit a second line of data in the stride?
  // (Datel's Wii FreeLoaders are the only titles known to set WPL to 0)
  bool potentially_interlaced_xfb =
      m_picture_configuration.WPL != 0 &&
      ((m_picture_configuration.STD / m_picture_configuration.WPL) == 2);
  // Are there an odd number of half-lines per field (definition of interlaced video)
  bool interlaced_video_mode = (GetHalfLinesPerEvenField() & 1) == 1;

  u32 fbStride = m_picture_configuration.STD * 16;
  u32 fbWidth = m_picture_configuration.WPL * 16;
  u32 fbHeight = m_vertical_timing_register.ACV;

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
    if (field == FieldType::Odd && m_vblank_timing_odd.PRB == m_vblank_timing_even.PRB + 1 &&
        xfbAddr)
    {
      xfbAddr -= fbStride;
    }

    if (field == FieldType::Even && m_vblank_timing_odd.PRB == m_vblank_timing_even.PRB - 1 &&
        xfbAddr)
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

void VideoInterfaceManager::BeginField(FieldType field, u64 ticks)
{
  // Outputting the frame at the beginning of scanout reduces latency. This assumes the game isn't
  // going to change the VI registers while a frame is scanning out.
  if (Config::Get(Config::GFX_HACK_EARLY_XFB_OUTPUT))
    OutputField(field, ticks);
}

void VideoInterfaceManager::EndField(FieldType field, u64 ticks)
{
  // If the game does change VI registers while a frame is scanning out, we can defer output
  // until the end so the last register values are used. This still isn't accurate, but it does
  // produce more acceptable results in some problematic cases.
  // Currently, this is only known to be necessary to eliminate flickering in WWE Crush Hour.
  if (!Config::Get(Config::GFX_HACK_EARLY_XFB_OUTPUT))
    OutputField(field, ticks);

  g_perf_metrics.CountVBlank();
  Core::OnFrameEnd();
}

// Purpose: Send VI interrupt when triggered
// Run when: When a frame is scanned (progressive/interlace)
void VideoInterfaceManager::Update(u64 ticks)
{
  // Movie's frame counter should be updated before actually rendering the frame,
  // in case frame counter display is enabled

  if (m_half_line_count == 0 || m_half_line_count == GetHalfLinesPerEvenField())
    Movie::FrameUpdate();

  // If this half-line is at some boundary of the "active video lines" in either field, we either
  // need to (a) send a request to the GPU thread to actually render the XFB, or (b) increment
  // the number of frames we've actually drawn

  if (m_half_line_count == m_even_field_first_hl)
  {
    BeginField(FieldType::Even, ticks);
  }
  else if (m_half_line_count == m_odd_field_first_hl)
  {
    BeginField(FieldType::Odd, ticks);
  }
  else if (m_half_line_count == m_even_field_last_hl)
  {
    EndField(FieldType::Even, ticks);
  }
  else if (m_half_line_count == m_odd_field_last_hl)
  {
    EndField(FieldType::Odd, ticks);
  }

  // If this half-line is at a field boundary, deal with frame stepping before potentially
  // dealing with SI polls, but after potentially sending a swap request to the GPU thread

  if (m_half_line_count == 0 || m_half_line_count == GetHalfLinesPerEvenField())
    Core::Callback_NewField(m_system);

  // If an SI poll is scheduled to happen on this half-line, do it!

  if (m_half_line_of_next_si_poll == m_half_line_count)
  {
    Core::UpdateInputGate(!Config::Get(Config::MAIN_INPUT_BACKGROUND_INPUT),
                          Config::Get(Config::MAIN_LOCK_CURSOR));
    auto& si = m_system.GetSerialInterface();
    si.UpdateDevices();
    m_half_line_of_next_si_poll += 2 * si.GetPollXLines();
  }

  // If this half-line is at the actual boundary of either field, schedule an SI poll to happen
  // some number of half-lines in the future

  if (m_half_line_count == 0)
  {
    m_half_line_of_next_si_poll = NUM_HALF_LINES_FOR_SI_POLL;  // first results start at vsync
  }
  if (m_half_line_count == GetHalfLinesPerEvenField())
  {
    m_half_line_of_next_si_poll = GetHalfLinesPerEvenField() + NUM_HALF_LINES_FOR_SI_POLL;
  }

  // Move to the next half-line and potentially roll-over the count to zero. If we've reached
  // the beginning of a new full-line, update the timer

  m_half_line_count++;
  if (m_half_line_count == GetHalfLinesPerEvenField() + GetHalfLinesPerOddField())
  {
    m_half_line_count = 0;
  }

  auto& core_timing = m_system.GetCoreTiming();
  if (!(m_half_line_count & 1))
  {
    m_ticks_last_line_start = core_timing.GetTicks();
  }

  // TODO: Findout why skipping interrupts acts as a frameskip
  if (core_timing.GetVISkip())
    return;

  // Check if we need to assert IR_INT. Note that the granularity of our current horizontal
  // position is limited to half-lines.

  for (UVIInterruptRegister& reg : m_interrupt_register)
  {
    u32 target_halfline = (reg.HCT > m_h_timing_0.HLW) ? 1 : 0;
    if ((1 + (m_half_line_count) / 2 == reg.VCT) && ((m_half_line_count & 1) == target_halfline))
    {
      reg.IR_INT = 1;
    }
  }

  UpdateInterrupts();
}

// Create a fake VI mode for a fifolog
void VideoInterfaceManager::FakeVIUpdate(u32 xfb_address, u32 fb_width, u32 fb_stride,
                                         u32 fb_height)
{
  bool interlaced = fb_height > 480 / 2;
  if (interlaced)
  {
    fb_height = fb_height / 2;
    fb_stride = fb_stride * 2;
  }

  m_xfb_info_top.POFF = 1;
  m_xfb_info_bottom.POFF = 1;
  m_vertical_timing_register.ACV = fb_height;
  m_vertical_timing_register.EQU = 6;
  m_vblank_timing_odd.PRB = 502 - fb_height * 2;
  m_vblank_timing_odd.PSB = 5;
  m_vblank_timing_even.PRB = 503 - fb_height * 2;
  m_vblank_timing_even.PSB = 4;
  m_picture_configuration.WPL = fb_width / 16;
  m_picture_configuration.STD = (fb_stride / 2) / 16;

  UpdateParameters();

  u32 total_halflines = GetHalfLinesPerEvenField() + GetHalfLinesPerOddField();

  if ((m_half_line_count - m_even_field_first_hl) % total_halflines <
      (m_half_line_count - m_odd_field_first_hl) % total_halflines)
  {
    // Even/Bottom field is next.
    m_xfb_info_bottom.FBB = interlaced ? (xfb_address + fb_width * 2) >> 5 : xfb_address >> 5;
  }
  else
  {
    // Odd/Top field is next
    m_xfb_info_top.FBB = (xfb_address >> 5);
  }
}

}  // namespace VideoInterface
