// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/VideoInterface.h"

#include <array>
#include <cmath>
#include <cstddef>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SystemTimers.h"

#include "DiscIO/Enums.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoInterface
{
// STATE_TO_SAVE
// Registers listed in order:
static UVIVerticalTimingRegister m_VerticalTimingRegister;
static UVIDisplayControlRegister m_DisplayControlRegister;
static UVIHorizontalTiming0 m_HTiming0;
static UVIHorizontalTiming1 m_HTiming1;
static UVIVBlankTimingRegister m_VBlankTimingOdd;
static UVIVBlankTimingRegister m_VBlankTimingEven;
static UVIBurstBlankingRegister m_BurstBlankingOdd;
static UVIBurstBlankingRegister m_BurstBlankingEven;
static UVIFBInfoRegister m_XFBInfoTop;
static UVIFBInfoRegister m_XFBInfoBottom;
static UVIFBInfoRegister m_3DFBInfoTop;  // Start making your stereoscopic demos! :p
static UVIFBInfoRegister m_3DFBInfoBottom;
static std::array<UVIInterruptRegister, 4> m_InterruptRegister;
static std::array<UVILatchRegister, 2> m_LatchRegister;
static PictureConfigurationRegister m_PictureConfiguration;
static UVIHorizontalScaling m_HorizontalScaling;
static SVIFilterCoefTables m_FilterCoefTables;
static u32 m_UnkAARegister = 0;  // ??? 0x00FF0000
static u16 m_Clock = 0;          // 0: 27MHz, 1: 54MHz
static UVIDTVStatus m_DTVStatus;
static UVIHorizontalStepping m_FBWidth;  // Only correct when scaling is enabled?
static UVIBorderBlankRegister m_BorderHBlank;
// 0xcc002076 - 0xcc00207f is full of 0x00FF: unknown
// 0xcc002080 - 0xcc002100 even more unknown

static u32 s_target_refresh_rate = 0;

static constexpr std::array<u32, 2> s_clock_freqs{{
    27000000, 54000000,
}};

static u64 s_ticks_last_line_start;  // number of ticks when the current full scanline started
static u32 s_half_line_count;        // number of halflines that have occurred for this full frame
static u32 s_half_line_of_next_si_poll;  // halfline when next SI poll results should be available
#if defined(_MSC_VER) && _MSC_VER <= 1800
static const u32 num_half_lines_for_si_poll = (7 * 2) + 1;  // this is how long an SI poll takes
#else
static constexpr u32 num_half_lines_for_si_poll = (7 * 2) + 1;  // this is how long an SI poll takes
#endif

static FieldType s_current_field;

// below indexes are 1-based
static u32 s_even_field_first_hl;  // index first halfline of the even field
static u32 s_odd_field_first_hl;   // index first halfline of the odd field
static u32 s_even_field_last_hl;   // index last halfline of the even field
static u32 s_odd_field_last_hl;    // index last halfline of the odd field

void DoState(PointerWrap& p)
{
  p.DoPOD(m_VerticalTimingRegister);
  p.DoPOD(m_DisplayControlRegister);
  p.Do(m_HTiming0);
  p.Do(m_HTiming1);
  p.Do(m_VBlankTimingOdd);
  p.Do(m_VBlankTimingEven);
  p.Do(m_BurstBlankingOdd);
  p.Do(m_BurstBlankingEven);
  p.Do(m_XFBInfoTop);
  p.Do(m_XFBInfoBottom);
  p.Do(m_3DFBInfoTop);
  p.Do(m_3DFBInfoBottom);
  p.DoArray(m_InterruptRegister);
  p.DoArray(m_LatchRegister);
  p.Do(m_PictureConfiguration);
  p.DoPOD(m_HorizontalScaling);
  p.Do(m_FilterCoefTables);
  p.Do(m_UnkAARegister);
  p.Do(m_Clock);
  p.Do(m_DTVStatus);
  p.Do(m_FBWidth);
  p.Do(m_BorderHBlank);
  p.Do(s_target_refresh_rate);
  p.Do(s_ticks_last_line_start);
  p.Do(s_half_line_count);
  p.Do(s_half_line_of_next_si_poll);
  p.Do(s_current_field);
  p.Do(s_even_field_first_hl);
  p.Do(s_odd_field_first_hl);
  p.Do(s_even_field_last_hl);
  p.Do(s_odd_field_last_hl);
}

// Executed after Init, before game boot
void Preset(bool _bNTSC)
{
  // NOTE: Make sure all registers are set to the correct initial state. The
  //	variables are not going to start zeroed if another game has been run
  //	previously (and mutated everything).

  m_VerticalTimingRegister.EQU = 6;
  m_VerticalTimingRegister.ACV = 0;

  m_DisplayControlRegister.ENB = 1;
  m_DisplayControlRegister.RST = 0;
  m_DisplayControlRegister.NIN = 0;
  m_DisplayControlRegister.DLR = 0;
  m_DisplayControlRegister.LE0 = 0;
  m_DisplayControlRegister.LE1 = 0;
  m_DisplayControlRegister.FMT = _bNTSC ? 0 : 1;

  m_HTiming0.HLW = 429;
  m_HTiming0.HCE = 105;
  m_HTiming0.HCS = 71;
  m_HTiming1.HSY = 64;
  m_HTiming1.HBE640 = 162;
  m_HTiming1.HBS640 = 373;

  m_VBlankTimingOdd.PRB = 502;
  m_VBlankTimingOdd.PSB = 5;
  m_VBlankTimingEven.PRB = 503;
  m_VBlankTimingEven.PSB = 4;

  m_BurstBlankingOdd.BS0 = 12;
  m_BurstBlankingOdd.BE0 = 520;
  m_BurstBlankingOdd.BS2 = 12;
  m_BurstBlankingOdd.BE2 = 520;
  m_BurstBlankingEven.BS0 = 13;
  m_BurstBlankingEven.BE0 = 519;
  m_BurstBlankingEven.BS2 = 13;
  m_BurstBlankingEven.BE2 = 519;

  m_XFBInfoTop.Hex = 0;
  m_XFBInfoBottom.Hex = 0;
  m_3DFBInfoTop.Hex = 0;
  m_3DFBInfoBottom.Hex = 0;

  m_InterruptRegister[0].HCT = 430;
  m_InterruptRegister[0].VCT = 263;
  m_InterruptRegister[0].IR_MASK = 1;
  m_InterruptRegister[0].IR_INT = 0;
  m_InterruptRegister[1].HCT = 1;
  m_InterruptRegister[1].VCT = 1;
  m_InterruptRegister[1].IR_MASK = 1;
  m_InterruptRegister[1].IR_INT = 0;
  m_InterruptRegister[2].Hex = 0;
  m_InterruptRegister[3].Hex = 0;

  m_LatchRegister = {};

  m_PictureConfiguration.STD = 40;
  m_PictureConfiguration.WPL = 40;

  m_HorizontalScaling.Hex = 0;
  m_FilterCoefTables = {};
  m_UnkAARegister = 0;

  DiscIO::Region region = SConfig::GetInstance().m_region;

  // 54MHz, capable of progressive scan
  m_Clock = DiscIO::IsNTSC(region);

  // Say component cable is plugged
  m_DTVStatus.component_plugged = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  m_DTVStatus.ntsc_j = region == DiscIO::Region::NTSC_J;

  m_FBWidth.Hex = 0;
  m_BorderHBlank.Hex = 0;

  s_ticks_last_line_start = 0;
  s_half_line_count = 1;
  s_half_line_of_next_si_poll = num_half_lines_for_si_poll;  // first sampling starts at vsync
  s_current_field = FieldType::Odd;

  UpdateParameters();
}

void Init()
{
  Preset(true);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  struct MappedVar
  {
    u32 addr;
    u16* ptr;
  };

  std::array<MappedVar, 46> directly_mapped_vars{{
      {VI_VERTICAL_TIMING, &m_VerticalTimingRegister.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &m_HTiming0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &m_HTiming0.Lo},
      {VI_HORIZONTAL_TIMING_1_HI, &m_HTiming1.Hi},
      {VI_HORIZONTAL_TIMING_1_LO, &m_HTiming1.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &m_VBlankTimingOdd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &m_VBlankTimingOdd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &m_VBlankTimingEven.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &m_VBlankTimingEven.Lo},
      {VI_BURST_BLANKING_ODD_HI, &m_BurstBlankingOdd.Hi},
      {VI_BURST_BLANKING_ODD_LO, &m_BurstBlankingOdd.Lo},
      {VI_BURST_BLANKING_EVEN_HI, &m_BurstBlankingEven.Hi},
      {VI_BURST_BLANKING_EVEN_LO, &m_BurstBlankingEven.Lo},
      {VI_FB_LEFT_TOP_LO, &m_XFBInfoTop.Lo},
      {VI_FB_RIGHT_TOP_LO, &m_3DFBInfoTop.Lo},
      {VI_FB_LEFT_BOTTOM_LO, &m_XFBInfoBottom.Lo},
      {VI_FB_RIGHT_BOTTOM_LO, &m_3DFBInfoBottom.Lo},
      {VI_PRERETRACE_LO, &m_InterruptRegister[0].Lo},
      {VI_POSTRETRACE_LO, &m_InterruptRegister[1].Lo},
      {VI_DISPLAY_INTERRUPT_2_LO, &m_InterruptRegister[2].Lo},
      {VI_DISPLAY_INTERRUPT_3_LO, &m_InterruptRegister[3].Lo},
      {VI_DISPLAY_LATCH_0_HI, &m_LatchRegister[0].Hi},
      {VI_DISPLAY_LATCH_0_LO, &m_LatchRegister[0].Lo},
      {VI_DISPLAY_LATCH_1_HI, &m_LatchRegister[1].Hi},
      {VI_DISPLAY_LATCH_1_LO, &m_LatchRegister[1].Lo},
      {VI_HSCALEW, &m_PictureConfiguration.Hex},
      {VI_HSCALER, &m_HorizontalScaling.Hex},
      {VI_FILTER_COEF_0_HI, &m_FilterCoefTables.Tables02[0].Hi},
      {VI_FILTER_COEF_0_LO, &m_FilterCoefTables.Tables02[0].Lo},
      {VI_FILTER_COEF_1_HI, &m_FilterCoefTables.Tables02[1].Hi},
      {VI_FILTER_COEF_1_LO, &m_FilterCoefTables.Tables02[1].Lo},
      {VI_FILTER_COEF_2_HI, &m_FilterCoefTables.Tables02[2].Hi},
      {VI_FILTER_COEF_2_LO, &m_FilterCoefTables.Tables02[2].Lo},
      {VI_FILTER_COEF_3_HI, &m_FilterCoefTables.Tables36[0].Hi},
      {VI_FILTER_COEF_3_LO, &m_FilterCoefTables.Tables36[0].Lo},
      {VI_FILTER_COEF_4_HI, &m_FilterCoefTables.Tables36[1].Hi},
      {VI_FILTER_COEF_4_LO, &m_FilterCoefTables.Tables36[1].Lo},
      {VI_FILTER_COEF_5_HI, &m_FilterCoefTables.Tables36[2].Hi},
      {VI_FILTER_COEF_5_LO, &m_FilterCoefTables.Tables36[2].Lo},
      {VI_FILTER_COEF_6_HI, &m_FilterCoefTables.Tables36[3].Hi},
      {VI_FILTER_COEF_6_LO, &m_FilterCoefTables.Tables36[3].Lo},
      {VI_CLOCK, &m_Clock},
      {VI_DTV_STATUS, &m_DTVStatus.Hex},
      {VI_FBWIDTH, &m_FBWidth.Hex},
      {VI_BORDER_BLANK_END, &m_BorderHBlank.Lo},
      {VI_BORDER_BLANK_START, &m_BorderHBlank.Hi},
  }};

  // Declare all the boilerplate direct MMIOs.
  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::DirectWrite<u16>(mapped_var.ptr));
  }

  std::array<MappedVar, 8> update_params_on_read_vars{{
      {VI_VERTICAL_TIMING, &m_VerticalTimingRegister.Hex},
      {VI_HORIZONTAL_TIMING_0_HI, &m_HTiming0.Hi},
      {VI_HORIZONTAL_TIMING_0_LO, &m_HTiming0.Lo},
      {VI_VBLANK_TIMING_ODD_HI, &m_VBlankTimingOdd.Hi},
      {VI_VBLANK_TIMING_ODD_LO, &m_VBlankTimingOdd.Lo},
      {VI_VBLANK_TIMING_EVEN_HI, &m_VBlankTimingEven.Hi},
      {VI_VBLANK_TIMING_EVEN_LO, &m_VBlankTimingEven.Lo},
      {VI_CLOCK, &m_Clock},
  }};

  // Declare all the MMIOs that update timing params.
  for (auto& mapped_var : update_params_on_read_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::ComplexWrite<u16>([mapped_var](u32, u16 val) {
                     *mapped_var.ptr = val;
                     UpdateParameters();
                   }));
  }

  // XFB related MMIOs that require special handling on writes.
  mmio->Register(base | VI_FB_LEFT_TOP_HI, MMIO::DirectRead<u16>(&m_XFBInfoTop.Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_XFBInfoTop.Hi = val;
                   if (m_XFBInfoTop.CLRPOFF)
                     m_XFBInfoTop.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_LEFT_BOTTOM_HI, MMIO::DirectRead<u16>(&m_XFBInfoBottom.Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_XFBInfoBottom.Hi = val;
                   if (m_XFBInfoBottom.CLRPOFF)
                     m_XFBInfoBottom.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_TOP_HI, MMIO::DirectRead<u16>(&m_3DFBInfoTop.Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_3DFBInfoTop.Hi = val;
                   if (m_3DFBInfoTop.CLRPOFF)
                     m_3DFBInfoTop.POFF = 0;
                 }));
  mmio->Register(base | VI_FB_RIGHT_BOTTOM_HI, MMIO::DirectRead<u16>(&m_3DFBInfoBottom.Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_3DFBInfoBottom.Hi = val;
                   if (m_3DFBInfoBottom.CLRPOFF)
                     m_3DFBInfoBottom.POFF = 0;
                 }));

  // MMIOs with unimplemented writes that trigger warnings.
  mmio->Register(
      base | VI_VERTICAL_BEAM_POSITION,
      MMIO::ComplexRead<u16>([](u32) { return 1 + (s_half_line_count - 1) / 2; }),
      MMIO::ComplexWrite<u16>([](u32, u16 val) {
        WARN_LOG(VIDEOINTERFACE,
                 "Changing vertical beam position to 0x%04x - not documented or implemented yet",
                 val);
      }));
  mmio->Register(
      base | VI_HORIZONTAL_BEAM_POSITION, MMIO::ComplexRead<u16>([](u32) {
        u16 value =
            static_cast<u16>(1 +
                             m_HTiming0.HLW * (CoreTiming::GetTicks() - s_ticks_last_line_start) /
                                 (GetTicksPerHalfLine()));
        return MathUtil::Clamp(value, static_cast<u16>(1), static_cast<u16>(m_HTiming0.HLW * 2));
      }),
      MMIO::ComplexWrite<u16>([](u32, u16 val) {
        WARN_LOG(VIDEOINTERFACE,
                 "Changing horizontal beam position to 0x%04x - not documented or implemented yet",
                 val);
      }));

  // The following MMIOs are interrupts related and update interrupt status
  // on writes.
  mmio->Register(base | VI_PRERETRACE_HI, MMIO::DirectRead<u16>(&m_InterruptRegister[0].Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_InterruptRegister[0].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_POSTRETRACE_HI, MMIO::DirectRead<u16>(&m_InterruptRegister[1].Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_InterruptRegister[1].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_2_HI,
                 MMIO::DirectRead<u16>(&m_InterruptRegister[2].Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_InterruptRegister[2].Hi = val;
                   UpdateInterrupts();
                 }));
  mmio->Register(base | VI_DISPLAY_INTERRUPT_3_HI,
                 MMIO::DirectRead<u16>(&m_InterruptRegister[3].Hi),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_InterruptRegister[3].Hi = val;
                   UpdateInterrupts();
                 }));

  // Unknown anti-aliasing related MMIO register: puts a warning on log and
  // needs to shift/mask when reading/writing.
  mmio->Register(base | VI_UNK_AA_REG_HI,
                 MMIO::ComplexRead<u16>([](u32) { return m_UnkAARegister >> 16; }),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_UnkAARegister = (m_UnkAARegister & 0x0000FFFF) | ((u32)val << 16);
                   WARN_LOG(VIDEOINTERFACE, "Writing to the unknown AA register (hi)");
                 }));
  mmio->Register(base | VI_UNK_AA_REG_LO,
                 MMIO::ComplexRead<u16>([](u32) { return m_UnkAARegister & 0xFFFF; }),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   m_UnkAARegister = (m_UnkAARegister & 0xFFFF0000) | val;
                   WARN_LOG(VIDEOINTERFACE, "Writing to the unknown AA register (lo)");
                 }));

  // Control register writes only updates some select bits, and additional
  // processing needs to be done if a reset is requested.
  mmio->Register(base | VI_CONTROL_REGISTER, MMIO::DirectRead<u16>(&m_DisplayControlRegister.Hex),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   UVIDisplayControlRegister tmpConfig(val);
                   m_DisplayControlRegister.ENB = tmpConfig.ENB;
                   m_DisplayControlRegister.NIN = tmpConfig.NIN;
                   m_DisplayControlRegister.DLR = tmpConfig.DLR;
                   m_DisplayControlRegister.LE0 = tmpConfig.LE0;
                   m_DisplayControlRegister.LE1 = tmpConfig.LE1;
                   m_DisplayControlRegister.FMT = tmpConfig.FMT;

                   if (tmpConfig.RST)
                   {
                     // shuffle2 clear all data, reset to default vals, and enter idle mode
                     m_DisplayControlRegister.RST = 0;
                     m_InterruptRegister = {};
                     UpdateInterrupts();
                   }

                   UpdateParameters();
                 }));

  // Map 8 bit reads (not writes) to 16 bit reads.
  for (int i = 0; i < 0x1000; i += 2)
  {
    mmio->Register(base | i, MMIO::ReadToLarger<u8>(mmio, base | i, 8), MMIO::InvalidWrite<u8>());
    mmio->Register(base | (i + 1), MMIO::ReadToLarger<u8>(mmio, base | i, 0),
                   MMIO::InvalidWrite<u8>());
  }

  // Map 32 bit reads and writes to 16 bit reads and writes.
  for (int i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
                   MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2)));
  }
}

void UpdateInterrupts()
{
  if ((m_InterruptRegister[0].IR_INT && m_InterruptRegister[0].IR_MASK) ||
      (m_InterruptRegister[1].IR_INT && m_InterruptRegister[1].IR_MASK) ||
      (m_InterruptRegister[2].IR_INT && m_InterruptRegister[2].IR_MASK) ||
      (m_InterruptRegister[3].IR_INT && m_InterruptRegister[3].IR_MASK))
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
  if (m_XFBInfoTop.POFF)
    return m_XFBInfoTop.FBB << 5;
  else
    return m_XFBInfoTop.FBB;
}

u32 GetXFBAddressBottom()
{
  // POFF for XFB bottom is connected to POFF for XFB top
  if (m_XFBInfoTop.POFF)
    return m_XFBInfoBottom.FBB << 5;
  else
    return m_XFBInfoBottom.FBB;
}

static u32 GetHalfLinesPerEvenField()
{
  return (3 * m_VerticalTimingRegister.EQU + m_VBlankTimingEven.PRB +
          2 * m_VerticalTimingRegister.ACV + m_VBlankTimingEven.PSB);
}

static u32 GetHalfLinesPerOddField()
{
  return (3 * m_VerticalTimingRegister.EQU + m_VBlankTimingOdd.PRB +
          2 * m_VerticalTimingRegister.ACV + m_VBlankTimingOdd.PSB);
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
  int active_lines = m_VerticalTimingRegister.ACV;
  int active_width_samples = (m_HTiming0.HLW + m_HTiming1.HBS640 - m_HTiming1.HBE640);

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
  if (m_DisplayControlRegister.FMT == 1)  // 625 line TV (PAL)
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
  if (std::isnormal(
          ratio))  // Check we have a sane ratio and haven't propagated any infs/nans/zeros
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
//     = (m_VerticalTimingRegister.EQU*3) half-scanlines
// R,r = preblanking
//     = (m_VBlankTimingX.PRB) half-scanlines
// A,a = active lines
//     = (m_VerticalTimingRegister.ACV*2) half-scanlines
// S,s = postblanking
//     = (m_VBlankTimingX.PSB) half-scanlines
//
// NB: for double-strike modes, the second field
//     does not get offset by half a scanline

void UpdateParameters()
{
  s_even_field_first_hl = 3 * m_VerticalTimingRegister.EQU + m_VBlankTimingEven.PRB + 1;
  s_odd_field_first_hl =
      GetHalfLinesPerEvenField() + 3 * m_VerticalTimingRegister.EQU + m_VBlankTimingOdd.PRB + 1;
  s_even_field_last_hl = s_even_field_first_hl + m_VerticalTimingRegister.ACV * 2;
  s_odd_field_last_hl = s_odd_field_first_hl + m_VerticalTimingRegister.ACV * 2;

  s_target_refresh_rate = lround(2.0 * SystemTimers::GetTicksPerSecond() /
                                 (GetTicksPerEvenField() + GetTicksPerOddField()));
}

u32 GetTargetRefreshRate()
{
  return s_target_refresh_rate;
}

u32 GetTicksPerSample()
{
  return 2 * SystemTimers::GetTicksPerSecond() / s_clock_freqs[m_Clock];
}

u32 GetTicksPerHalfLine()
{
  return GetTicksPerSample() * m_HTiming0.HLW;
}

u32 GetTicksPerField()
{
  return GetTicksPerEvenField();
}

u32 GetTicksPerFrame()
{
  // todo: check if this is right
  return GetTicksPerEvenField() + GetTicksPerOddField();
}

static void LogField(FieldType field, u32 xfb_address)
{
  static constexpr std::array<const char*, 2> field_type_names{{"Odd", "Even"}};

  static const std::array<const UVIVBlankTimingRegister*, 2> vert_timing{{
      &m_VBlankTimingOdd, &m_VBlankTimingEven,
  }};

  const auto field_index = static_cast<size_t>(field);

  DEBUG_LOG(VIDEOINTERFACE, "(VI->BeginField): Address: %.08X | WPL %u | STD %u | EQ %u | PRB %u | "
                            "ACV %u | PSB %u | Field %s",
            xfb_address, m_PictureConfiguration.WPL, m_PictureConfiguration.STD,
            m_VerticalTimingRegister.EQU, vert_timing[field_index]->PRB,
            m_VerticalTimingRegister.ACV, vert_timing[field_index]->PSB,
            field_type_names[field_index]);

  DEBUG_LOG(VIDEOINTERFACE, "HorizScaling: %04x | fbwidth %d | %u | %u", m_HorizontalScaling.Hex,
            m_FBWidth.Hex, GetTicksPerEvenField(), GetTicksPerOddField());
}

static void BeginField(FieldType field, u64 ticks)
{
  // Could we fit a second line of data in the stride?
  bool potentially_interlaced_xfb =
      ((m_PictureConfiguration.STD / m_PictureConfiguration.WPL) == 2);
  // Are there an odd number of half-lines per field (definition of interlaced video)
  bool interlaced_video_mode = (GetHalfLinesPerEvenField() & 1) == 1;

  u32 fbStride = m_PictureConfiguration.STD * 16;
  u32 fbWidth = m_PictureConfiguration.WPL * 16;
  u32 fbHeight = m_VerticalTimingRegister.ACV;

  u32 xfbAddr;

  if (field == FieldType::Even)
  {
    xfbAddr = GetXFBAddressBottom();
  }
  else
  {
    xfbAddr = GetXFBAddressTop();
  }

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
    if (field == FieldType::Odd && m_VBlankTimingOdd.PRB == m_VBlankTimingEven.PRB + 1 && xfbAddr)
      xfbAddr -= fbStride * 2;

    if (field == FieldType::Even && m_VBlankTimingOdd.PRB == m_VBlankTimingEven.PRB - 1 && xfbAddr)
      xfbAddr -= fbStride * 2;
  }

  LogField(field, xfbAddr);

  // This assumes the game isn't going to change the VI registers while a
  // frame is scanning out.
  // To correctly handle that case we would need to collate all changes
  // to VI during scanout and delay outputting the frame till then.
  if (xfbAddr)
    g_video_backend->Video_BeginField(xfbAddr, fbWidth, fbStride, fbHeight, ticks);
}

static void EndField()
{
  Core::VideoThrottle();
}

// Purpose: Send VI interrupt when triggered
// Run when: When a frame is scanned (progressive/interlace)
void Update(u64 ticks)
{
  if (s_half_line_of_next_si_poll == s_half_line_count)
  {
    SerialInterface::UpdateDevices();
    s_half_line_of_next_si_poll += SerialInterface::GetPollXLines();
  }
  if (s_half_line_count == s_even_field_first_hl)
  {
    BeginField(FieldType::Even, ticks);
  }
  else if (s_half_line_count == s_odd_field_first_hl)
  {
    BeginField(FieldType::Odd, ticks);
  }
  else if (s_half_line_count == s_even_field_last_hl)
  {
    EndField();
  }
  else if (s_half_line_count == s_odd_field_last_hl)
  {
    EndField();
  }

  for (UVIInterruptRegister& reg : m_InterruptRegister)
  {
    if (s_half_line_count + 1 == 2u * reg.VCT)
    {
      reg.IR_INT = 1;
    }
  }

  s_half_line_count++;

  if (s_half_line_count > GetHalfLinesPerEvenField() + GetHalfLinesPerOddField())
  {
    s_half_line_count = 1;
    s_half_line_of_next_si_poll = num_half_lines_for_si_poll;  // first results start at vsync
  }

  if (s_half_line_count == GetHalfLinesPerEvenField())
  {
    s_half_line_of_next_si_poll = GetHalfLinesPerEvenField() + num_half_lines_for_si_poll;
  }

  if (s_half_line_count & 1)
  {
    s_ticks_last_line_start = CoreTiming::GetTicks();
  }

  UpdateInterrupts();
}

}  // namespace
