// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/State.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/VideoBackendBase.h"

namespace VideoInterface
{

// STATE_TO_SAVE
// Registers listed in order:
static UVIVerticalTimingRegister m_VerticalTimingRegister;
static UVIDisplayControlRegister m_DisplayControlRegister;
static UVIHorizontalTiming0      m_HTiming0;
static UVIHorizontalTiming1      m_HTiming1;
static UVIVBlankTimingRegister   m_VBlankTimingOdd;
static UVIVBlankTimingRegister   m_VBlankTimingEven;
static UVIBurstBlankingRegister  m_BurstBlankingOdd;
static UVIBurstBlankingRegister  m_BurstBlankingEven;
static UVIFBInfoRegister         m_XFBInfoTop;
static UVIFBInfoRegister         m_XFBInfoBottom;
static UVIFBInfoRegister         m_3DFBInfoTop;     // Start making your stereoscopic demos! :p
static UVIFBInfoRegister         m_3DFBInfoBottom;
static u16                       m_VBeamPos = 0;    // 0: Inactive
static u16                       m_HBeamPos = 0;    // 0: Inactive
static UVIInterruptRegister      m_InterruptRegister[4];
static UVILatchRegister          m_LatchRegister[2];
static PictureConfigurationRegister  m_PictureConfiguration;
static UVIHorizontalScaling      m_HorizontalScaling;
static SVIFilterCoefTables       m_FilterCoefTables;
static u32                       m_UnkAARegister = 0;// ??? 0x00FF0000
static u16                       m_Clock = 0;       // 0: 27MHz, 1: 54MHz
static UVIDTVStatus              m_DTVStatus;
static UVIHorizontalStepping     m_FBWidth;         // Only correct when scaling is enabled?
static UVIBorderBlankRegister    m_BorderHBlank;
// 0xcc002076 - 0xcc00207f is full of 0x00FF: unknown
// 0xcc002080 - 0xcc002100 even more unknown

u32 TargetRefreshRate = 0;

static u32 TicksPerFrame = 0;
static u32 s_lineCount = 0;
static u32 s_upperFieldBegin = 0;
static u32 s_lowerFieldBegin = 0;
static int fields = 1;

void DoState(PointerWrap &p)
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
	p.Do(m_VBeamPos);
	p.Do(m_HBeamPos);
	p.DoArray(m_InterruptRegister, 4);
	p.DoArray(m_LatchRegister, 2);
	p.Do(m_PictureConfiguration);
	p.DoPOD(m_HorizontalScaling);
	p.Do(m_FilterCoefTables);
	p.Do(m_UnkAARegister);
	p.Do(m_Clock);
	p.Do(m_DTVStatus);
	p.Do(m_FBWidth);
	p.Do(m_BorderHBlank);
	p.Do(TargetRefreshRate);
	p.Do(TicksPerFrame);
	p.Do(s_lineCount);
	p.Do(s_upperFieldBegin);
	p.Do(s_lowerFieldBegin);
}

// Executed after Init, before game boot
void Preset(bool _bNTSC)
{
	m_VerticalTimingRegister.EQU = 6;

	m_DisplayControlRegister.ENB = 1;
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

	m_InterruptRegister[0].HCT = 430;
	m_InterruptRegister[0].VCT = 263;
	m_InterruptRegister[0].IR_MASK = 1;
	m_InterruptRegister[0].IR_INT = 0;
	m_InterruptRegister[1].HCT = 1;
	m_InterruptRegister[1].VCT = 1;
	m_InterruptRegister[1].IR_MASK = 1;
	m_InterruptRegister[1].IR_INT = 0;

	m_PictureConfiguration.STD = 40;
	m_PictureConfiguration.WPL = 40;

	m_HBeamPos = -1; // NTSC-U N64 VC games check for a non-zero HBeamPos
	m_VBeamPos = 0; // RG4JC0 checks for a zero VBeamPos

	// 54MHz, capable of progressive scan
	m_Clock = SConfig::GetInstance().bNTSC;

	// Say component cable is plugged
	m_DTVStatus.component_plugged = SConfig::GetInstance().bProgressive;

	UpdateParameters();
}

void Init()
{
	m_VerticalTimingRegister.Hex = 0;
	m_DisplayControlRegister.Hex = 0;
	m_HTiming0.Hex = 0;
	m_HTiming1.Hex = 0;
	m_VBlankTimingOdd.Hex = 0;
	m_VBlankTimingEven.Hex = 0;
	m_BurstBlankingOdd.Hex = 0;
	m_BurstBlankingEven.Hex = 0;
	m_XFBInfoTop.Hex = 0;
	m_XFBInfoBottom.Hex = 0;
	m_3DFBInfoTop.Hex = 0;
	m_3DFBInfoBottom.Hex = 0;
	m_VBeamPos = 0;
	m_HBeamPos = 0;
	m_PictureConfiguration.Hex = 0;
	m_HorizontalScaling.Hex = 0;
	m_UnkAARegister = 0;
	m_Clock = 0;
	m_DTVStatus.Hex = 0;
	m_FBWidth.Hex = 0;
	m_BorderHBlank.Hex = 0;
	memset(&m_FilterCoefTables, 0, sizeof(m_FilterCoefTables));

	fields = 1;

	m_DTVStatus.ntsc_j = SConfig::GetInstance().bForceNTSCJ;

	for (UVIInterruptRegister& reg : m_InterruptRegister)
	{
		reg.Hex = 0;
	}

	for (UVILatchRegister& reg : m_LatchRegister)
	{
		reg.Hex = 0;
	}

	m_DisplayControlRegister.Hex = 0;
	UpdateParameters();
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	struct {
		u32 addr;
		u16* ptr;
	} directly_mapped_vars[] = {
		{ VI_VERTICAL_TIMING, &m_VerticalTimingRegister.Hex },
		{ VI_HORIZONTAL_TIMING_0_HI, &m_HTiming0.Hi },
		{ VI_HORIZONTAL_TIMING_0_LO, &m_HTiming0.Lo },
		{ VI_HORIZONTAL_TIMING_1_HI, &m_HTiming1.Hi },
		{ VI_HORIZONTAL_TIMING_1_LO, &m_HTiming1.Lo },
		{ VI_VBLANK_TIMING_ODD_HI, &m_VBlankTimingOdd.Hi },
		{ VI_VBLANK_TIMING_ODD_LO, &m_VBlankTimingOdd.Lo },
		{ VI_VBLANK_TIMING_EVEN_HI, &m_VBlankTimingEven.Hi },
		{ VI_VBLANK_TIMING_EVEN_LO, &m_VBlankTimingEven.Lo },
		{ VI_BURST_BLANKING_ODD_HI, &m_BurstBlankingOdd.Hi },
		{ VI_BURST_BLANKING_ODD_LO, &m_BurstBlankingOdd.Lo },
		{ VI_BURST_BLANKING_EVEN_HI, &m_BurstBlankingEven.Hi },
		{ VI_BURST_BLANKING_EVEN_LO, &m_BurstBlankingEven.Lo },
		{ VI_FB_LEFT_TOP_LO, &m_XFBInfoTop.Lo },
		{ VI_FB_RIGHT_TOP_LO, &m_3DFBInfoTop.Lo },
		{ VI_FB_LEFT_BOTTOM_LO, &m_XFBInfoBottom.Lo },
		{ VI_FB_RIGHT_BOTTOM_LO, &m_3DFBInfoBottom.Lo },
		{ VI_PRERETRACE_LO, &m_InterruptRegister[0].Lo },
		{ VI_POSTRETRACE_LO, &m_InterruptRegister[1].Lo },
		{ VI_DISPLAY_INTERRUPT_2_LO, &m_InterruptRegister[2].Lo },
		{ VI_DISPLAY_INTERRUPT_3_LO, &m_InterruptRegister[3].Lo },
		{ VI_DISPLAY_LATCH_0_HI, &m_LatchRegister[0].Hi },
		{ VI_DISPLAY_LATCH_0_LO, &m_LatchRegister[0].Lo },
		{ VI_DISPLAY_LATCH_1_HI, &m_LatchRegister[1].Hi },
		{ VI_DISPLAY_LATCH_1_LO, &m_LatchRegister[1].Lo },
		{ VI_HSCALEW, &m_PictureConfiguration.Hex },
		{ VI_HSCALER, &m_HorizontalScaling.Hex },
		{ VI_FILTER_COEF_0_HI, &m_FilterCoefTables.Tables02[0].Hi },
		{ VI_FILTER_COEF_0_LO, &m_FilterCoefTables.Tables02[0].Lo },
		{ VI_FILTER_COEF_1_HI, &m_FilterCoefTables.Tables02[1].Hi },
		{ VI_FILTER_COEF_1_LO, &m_FilterCoefTables.Tables02[1].Lo },
		{ VI_FILTER_COEF_2_HI, &m_FilterCoefTables.Tables02[2].Hi },
		{ VI_FILTER_COEF_2_LO, &m_FilterCoefTables.Tables02[2].Lo },
		{ VI_FILTER_COEF_3_HI, &m_FilterCoefTables.Tables36[0].Hi },
		{ VI_FILTER_COEF_3_LO, &m_FilterCoefTables.Tables36[0].Lo },
		{ VI_FILTER_COEF_4_HI, &m_FilterCoefTables.Tables36[1].Hi },
		{ VI_FILTER_COEF_4_LO, &m_FilterCoefTables.Tables36[1].Lo },
		{ VI_FILTER_COEF_5_HI, &m_FilterCoefTables.Tables36[2].Hi },
		{ VI_FILTER_COEF_5_LO, &m_FilterCoefTables.Tables36[2].Lo },
		{ VI_FILTER_COEF_6_HI, &m_FilterCoefTables.Tables36[3].Hi },
		{ VI_FILTER_COEF_6_LO, &m_FilterCoefTables.Tables36[3].Lo },
		{ VI_CLOCK, &m_Clock },
		{ VI_DTV_STATUS, &m_DTVStatus.Hex },
		{ VI_FBWIDTH, &m_FBWidth.Hex },
		{ VI_BORDER_BLANK_END, &m_BorderHBlank.Lo },
		{ VI_BORDER_BLANK_START, &m_BorderHBlank.Hi },
	};

	// Declare all the boilerplate direct MMIOs.
	for (auto& mapped_var : directly_mapped_vars)
	{
		mmio->Register(base | mapped_var.addr,
			MMIO::DirectRead<u16>(mapped_var.ptr),
			MMIO::DirectWrite<u16>(mapped_var.ptr)
		);
	}

	// XFB related MMIOs that require special handling on writes.
	mmio->Register(base | VI_FB_LEFT_TOP_HI,
		MMIO::DirectRead<u16>(&m_XFBInfoTop.Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_XFBInfoTop.Hi = val;
			if (m_XFBInfoTop.CLRPOFF) m_XFBInfoTop.POFF = 0;
		})
	);
	mmio->Register(base | VI_FB_LEFT_BOTTOM_HI,
		MMIO::DirectRead<u16>(&m_XFBInfoBottom.Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_XFBInfoBottom.Hi = val;
			if (m_XFBInfoBottom.CLRPOFF) m_XFBInfoBottom.POFF = 0;
		})
	);
	mmio->Register(base | VI_FB_RIGHT_TOP_HI,
		MMIO::DirectRead<u16>(&m_3DFBInfoTop.Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_3DFBInfoTop.Hi = val;
			if (m_3DFBInfoTop.CLRPOFF) m_3DFBInfoTop.POFF = 0;
		})
	);
	mmio->Register(base | VI_FB_RIGHT_BOTTOM_HI,
		MMIO::DirectRead<u16>(&m_3DFBInfoBottom.Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_3DFBInfoBottom.Hi = val;
			if (m_3DFBInfoBottom.CLRPOFF) m_3DFBInfoBottom.POFF = 0;
		})
	);

	// MMIOs with unimplemented writes that trigger warnings.
	mmio->Register(base | VI_VERTICAL_BEAM_POSITION,
		MMIO::DirectRead<u16>(&m_VBeamPos),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			WARN_LOG(VIDEOINTERFACE, "Changing vertical beam position to 0x%04x - not documented or implemented yet", val);
		})
	);
	mmio->Register(base | VI_HORIZONTAL_BEAM_POSITION,
		MMIO::DirectRead<u16>(&m_HBeamPos),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			WARN_LOG(VIDEOINTERFACE, "Changing horizontal beam position to 0x%04x - not documented or implemented yet", val);
		})
	);

	// The following MMIOs are interrupts related and update interrupt status
	// on writes.
	mmio->Register(base | VI_PRERETRACE_HI,
		MMIO::DirectRead<u16>(&m_InterruptRegister[0].Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_InterruptRegister[0].Hi = val;
			UpdateInterrupts();
		})
	);
	mmio->Register(base | VI_POSTRETRACE_HI,
		MMIO::DirectRead<u16>(&m_InterruptRegister[1].Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_InterruptRegister[1].Hi = val;
			UpdateInterrupts();
		})
	);
	mmio->Register(base | VI_DISPLAY_INTERRUPT_2_HI,
		MMIO::DirectRead<u16>(&m_InterruptRegister[2].Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_InterruptRegister[2].Hi = val;
			UpdateInterrupts();
		})
	);
	mmio->Register(base | VI_DISPLAY_INTERRUPT_3_HI,
		MMIO::DirectRead<u16>(&m_InterruptRegister[3].Hi),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_InterruptRegister[3].Hi = val;
			UpdateInterrupts();
		})
	);

	// Unknown anti-aliasing related MMIO register: puts a warning on log and
	// needs to shift/mask when reading/writing.
	mmio->Register(base | VI_UNK_AA_REG_HI,
		MMIO::ComplexRead<u16>([](u32) {
			return m_UnkAARegister >> 16;
		}),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_UnkAARegister = (m_UnkAARegister & 0x0000FFFF) | ((u32)val << 16);
			WARN_LOG(VIDEOINTERFACE, "Writing to the unknown AA register (hi)");
		})
	);
	mmio->Register(base | VI_UNK_AA_REG_LO,
		MMIO::ComplexRead<u16>([](u32) {
			return m_UnkAARegister & 0xFFFF;
		}),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			m_UnkAARegister = (m_UnkAARegister & 0xFFFF0000) | val;
			WARN_LOG(VIDEOINTERFACE, "Writing to the unknown AA register (lo)");
		})
	);

	// Control register writes only updates some select bits, and additional
	// processing needs to be done if a reset is requested.
	mmio->Register(base | VI_CONTROL_REGISTER,
		MMIO::DirectRead<u16>(&m_DisplayControlRegister.Hex),
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
				for (UVIInterruptRegister& reg : m_InterruptRegister)
				{
					reg.Hex = 0;
				}
				UpdateInterrupts();
			}

			UpdateParameters();
		})
	);

	// Map 8 bit reads (not writes) to 16 bit reads.
	for (int i = 0; i < 0x1000; i += 2)
	{
		mmio->Register(base | i,
			MMIO::ReadToLarger<u8>(mmio, base | i, 8),
			MMIO::InvalidWrite<u8>()
		);
		mmio->Register(base | (i + 1),
			MMIO::ReadToLarger<u8>(mmio, base | i, 0),
			MMIO::InvalidWrite<u8>()
		);
	}

	// Map 32 bit reads and writes to 16 bit reads and writes.
	for (int i = 0; i < 0x1000; i += 4)
	{
		mmio->Register(base | i,
			MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
			MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2))
		);
	}
}

void SetRegionReg(char region)
{
	if (!SConfig::GetInstance().bForceNTSCJ)
		m_DTVStatus.ntsc_j = region == 'J';
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

float GetAspectRatio(bool wide)
{
	u32 multiplier = static_cast<u32>(m_PictureConfiguration.STD / m_PictureConfiguration.WPL);
	int height = (multiplier * m_VerticalTimingRegister.ACV);
	int width = ((2 * m_HTiming0.HLW) - (m_HTiming0.HLW - m_HTiming1.HBS640)
		- m_HTiming1.HBE640);
	float pixelAR;
	if (m_DisplayControlRegister.FMT == 1)
	{
		//PAL active frame is 702*576
		//In square pixels, 1024*576 is 16:9, and 768*576 is 4:3
		//Therefore a 16:9 TV would have a "pixel" aspect ratio of 1024/702
		//Similarly a 4:3 TV would have a ratio of 768/702
		if (wide)
		{
			pixelAR = 1024.0f / 702.0f;
		}
		else
		{
			pixelAR = 768.0f / 702.0f;
		}
	}
	else
	{
		//NTSC active frame is 710.85*486
		//In square pixels, 864*486 is 16:9, and 648*486 is 4:3
		//Therefore a 16:9 TV would have a "pixel" aspect ratio of 864/710.85
		//Similarly a 4:3 TV would have a ratio of 648/710.85
		if (wide)
		{
			pixelAR = 864.0f / 710.85f;
		}
		else
		{
			pixelAR = 648.0f / 710.85f;
		}
	}
	if (width == 0 || height == 0)
	{
		if (wide)
		{
			return 16.0f/9.0f;
		}
		else
		{
			return 4.0f/3.0f;
		}
	}
	return ((float)width / (float)height) * pixelAR;
}

void UpdateParameters()
{
	fields = m_DisplayControlRegister.NIN ? 2 : 1;

	switch (m_DisplayControlRegister.FMT)
	{
	case 0: // NTSC
		TargetRefreshRate = NTSC_FIELD_RATE;
		TicksPerFrame = SystemTimers::GetTicksPerSecond() / NTSC_FIELD_RATE;
		s_lineCount = NTSC_LINE_COUNT;
		s_upperFieldBegin = NTSC_UPPER_BEGIN;
		s_lowerFieldBegin = NTSC_LOWER_BEGIN;
		break;

	case 2: // PAL-M
		TargetRefreshRate = NTSC_FIELD_RATE;
		TicksPerFrame = SystemTimers::GetTicksPerSecond() / NTSC_FIELD_RATE;
		s_lineCount = PAL_LINE_COUNT;
		s_upperFieldBegin = PAL_UPPER_BEGIN;
		s_lowerFieldBegin = PAL_LOWER_BEGIN;
		break;

	case 1: // PAL
		TargetRefreshRate = PAL_FIELD_RATE;
		TicksPerFrame = SystemTimers::GetTicksPerSecond() / PAL_FIELD_RATE;
		s_lineCount = PAL_LINE_COUNT;
		s_upperFieldBegin = PAL_UPPER_BEGIN;
		s_lowerFieldBegin = PAL_LOWER_BEGIN;
		break;

	case 3: // Debug
		PanicAlert("Debug video mode not implemented");
		break;

	default:
		PanicAlert("Unknown Video Format - CVideoInterface");
		break;
	}
}

unsigned int GetTicksPerLine()
{
	if (s_lineCount == 0)
	{
		return 1;
	}
	else
	{
		return TicksPerFrame / (s_lineCount / (2 / fields)) ;
	}
}

unsigned int GetTicksPerFrame()
{
	return TicksPerFrame;
}

static void BeginField(FieldType field)
{
	// TODO: the stride and height shouldn't depend on whether the FieldType is
	// "progressive".  We're actually telling the video backend to draw unspecified
	// junk.  Due to the way XFB copies work, the unspecified junk is usually
	// the contents of the other field, so it looks okay in most cases, but we
	// shouldn't depend on that; a good example of where our output is wrong is
	// the title screen teaser videos in Metroid Prime.
	//
	// What should actually happen is that we should pass on the correct width,
	// stride, and height to the video backend, and it should deinterlace the
	// output when appropriate.
	u32 fbStride = m_PictureConfiguration.STD * (field == FIELD_PROGRESSIVE ? 16 : 8);
	u32 fbWidth = m_PictureConfiguration.WPL * 16;
	u32 fbHeight = m_VerticalTimingRegister.ACV * (field == FIELD_PROGRESSIVE ? 1 : 2);
	u32 xfbAddr;

	// Only the top field is valid in progressive mode.
	if (field == FieldType::FIELD_PROGRESSIVE)
	{
		xfbAddr = GetXFBAddressTop();
	}
	else
	{
		// If we are displaying an interlaced field, we convert it to a progressive field
		// by simply using the whole XFB, which 99% of the time contains a whole progressive
		// frame.

		// All known NTSC games use the top/odd field as the upper field but
		// PAL games are known to whimsically arrange the fields in either order.
		// So to work out which field is pointing to the top of the progressive XFB we check
		// which field has the lower PRB value in the VBlank Timing Registers.

		if(m_VBlankTimingOdd.PRB < m_VBlankTimingEven.PRB)
			xfbAddr = GetXFBAddressTop();
		else
			xfbAddr = GetXFBAddressBottom();
	}

	static const char* const fieldTypeNames[] = { "Progressive", "Upper", "Lower" };

	DEBUG_LOG(VIDEOINTERFACE,
			  "(VI->BeginField): Address: %.08X | WPL %u | STD %u | ACV %u | Field %s",
			  xfbAddr, m_PictureConfiguration.WPL, m_PictureConfiguration.STD,
			  m_VerticalTimingRegister.ACV, fieldTypeNames[field]);

	if (xfbAddr)
		g_video_backend->Video_BeginField(xfbAddr, fbWidth, fbStride, fbHeight);
}

static void EndField()
{
	g_video_backend->Video_EndField();
	Core::VideoThrottle();
}

// Purpose: Send VI interrupt when triggered
// Run when: When a frame is scanned (progressive/interlace)
void Update()
{
	if (m_DisplayControlRegister.NIN)
	{
		// Progressive
		if (m_VBeamPos == 1)
			BeginField(FIELD_PROGRESSIVE);
	}
	else if (m_VBeamPos == s_upperFieldBegin)
	{
		// Interlace Upper
		BeginField(FIELD_UPPER);
	}
	else if (m_VBeamPos == s_lowerFieldBegin)
	{
		// Interlace Lower
		BeginField(FIELD_LOWER);
	}

	if (m_VBeamPos == s_upperFieldBegin + m_VerticalTimingRegister.ACV)
	{
		// Interlace Upper.
		EndField();
	}
	else if (m_VBeamPos == s_lowerFieldBegin + m_VerticalTimingRegister.ACV)
	{
		// Interlace Lower
		EndField();
	}

	if (++m_VBeamPos > s_lineCount * fields)
		m_VBeamPos = 1;

	for (UVIInterruptRegister& reg : m_InterruptRegister)
	{
		if (m_VBeamPos == reg.VCT)
		{
			reg.IR_INT = 1;
		}
	}
	UpdateInterrupts();
}

} // namespace
