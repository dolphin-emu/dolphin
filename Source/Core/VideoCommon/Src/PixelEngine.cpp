// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// http://developer.nvidia.com/object/General_FAQ.html#t6 !!!!!


#include "Common.h"
#include "VideoCommon.h"
#include "ChunkFile.h"
#include "Atomic.h"
#include "CoreTiming.h"
#include "ConfigManager.h"

#include "PixelEngine.h"
#include "RenderBase.h"
#include "CommandProcessor.h"
#include "HW/ProcessorInterface.h"
#include "DLCache.h"
#include "State.h"

namespace PixelEngine
{

union UPEZConfReg
{
	u16 Hex;
	struct 
	{
		u16 ZCompEnable		: 1; // Z Comparator Enable
		u16 Function		: 3;
		u16 ZUpdEnable		: 1;
		u16					: 11;
	};
};

union UPEAlphaConfReg
{
	u16 Hex;
	struct 
	{
		u16 BMMath			: 1; // GX_BM_BLEND || GX_BM_SUBSTRACT
		u16 BMLogic			: 1; // GX_BM_LOGIC
		u16 Dither			: 1;
		u16 ColorUpdEnable	: 1;
		u16 AlphaUpdEnable	: 1;
		u16 DstFactor		: 3;
		u16 SrcFactor		: 3;
		u16 Substract		: 1; // Additive mode by default
		u16 BlendOperator	: 4;
	};
};

union UPEDstAlphaConfReg
{
	u16 Hex;
	struct 
	{
		u16 DstAlpha		: 8;
		u16 Enable			: 1;
		u16					: 7;
	};
};

union UPEAlphaModeConfReg
{
	u16 Hex;
	struct 
	{
		u16 Threshold		: 8;
		u16 CompareMode		: 8;
	};
};

// fifo Control Register
union UPECtrlReg
{
	struct 
	{
		u16 PETokenEnable	:	1;
		u16 PEFinishEnable	:	1;
		u16 PEToken			:	1; // write only
		u16 PEFinish		:	1; // write only
		u16					:	12;
	};
	u16 Hex;
	UPECtrlReg() {Hex = 0; }
	UPECtrlReg(u16 _hex) {Hex = _hex; }
};

// STATE_TO_SAVE
static UPEZConfReg			m_ZConf;
static UPEAlphaConfReg		m_AlphaConf;
static UPEDstAlphaConfReg	m_DstAlphaConf;
static UPEAlphaModeConfReg	m_AlphaModeConf;
static UPEAlphaReadReg		m_AlphaRead;
static UPECtrlReg			m_Control;
//static u16					m_Token; // token value most recently encountered

volatile u32 g_bSignalTokenInterrupt;
volatile u32 g_bSignalFinishInterrupt;

static int et_SetTokenOnMainThread;
static int et_SetFinishOnMainThread;

volatile u32 interruptSetToken = 0;
volatile u32 interruptSetFinish = 0;

u16 bbox[4];
bool bbox_active;

enum
{
	INT_CAUSE_PE_TOKEN    =  0x200, // GP Token
	INT_CAUSE_PE_FINISH   =  0x400, // GP Finished
};

void DoState(PointerWrap &p)
{
	p.Do(m_ZConf);
	p.Do(m_AlphaConf);
	p.Do(m_DstAlphaConf);
	p.Do(m_AlphaModeConf);
	p.Do(m_AlphaRead);
	p.DoPOD(m_Control);

	p.Do(g_bSignalTokenInterrupt);
	p.Do(g_bSignalFinishInterrupt);
	p.Do(interruptSetToken);
	p.Do(interruptSetFinish);
	
	p.Do(bbox);
	p.Do(bbox_active);
}

void UpdateInterrupts();
void UpdateTokenInterrupt(bool active);
void UpdateFinishInterrupt(bool active);
void SetToken_OnMainThread(u64 userdata, int cyclesLate);
void SetFinish_OnMainThread(u64 userdata, int cyclesLate);

void Init()
{
	m_Control.Hex = 0;
	m_ZConf.Hex = 0;
	m_AlphaConf.Hex = 0;
	m_DstAlphaConf.Hex = 0;
	m_AlphaModeConf.Hex = 0;
	m_AlphaRead.Hex = 0;

	g_bSignalTokenInterrupt = 0;
	g_bSignalFinishInterrupt = 0;
	interruptSetToken = 0;
	interruptSetFinish = 0;

	et_SetTokenOnMainThread = CoreTiming::RegisterEvent("SetToken", SetToken_OnMainThread);
	et_SetFinishOnMainThread = CoreTiming::RegisterEvent("SetFinish", SetFinish_OnMainThread);

	bbox[0] = 0x80;
	bbox[1] = 0xA0;
	bbox[2] = 0x80;
	bbox[3] = 0xA0;

	bbox_active = false;
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	DEBUG_LOG(PIXELENGINE, "(r16) 0x%08x", _iAddress);
	switch (_iAddress & 0xFFF)
	{
		// CPU Direct Access EFB Raster State Config
	case PE_ZCONF:
		_uReturnValue = m_ZConf.Hex;
		INFO_LOG(PIXELENGINE, "(r16) ZCONF");
		break;
	case PE_ALPHACONF:
		// Most games read this early. no idea why.
		_uReturnValue = m_AlphaConf.Hex;
		INFO_LOG(PIXELENGINE, "(r16) ALPHACONF");
		break;
	case PE_DSTALPHACONF:
		_uReturnValue = m_DstAlphaConf.Hex;
		INFO_LOG(PIXELENGINE, "(r16) DSTALPHACONF");
		break;
	case PE_ALPHAMODE:
		_uReturnValue = m_AlphaModeConf.Hex;
		INFO_LOG(PIXELENGINE, "(r16) ALPHAMODE");
		break;	
	case PE_ALPHAREAD:
		_uReturnValue = m_AlphaRead.Hex;
		WARN_LOG(PIXELENGINE, "(r16) ALPHAREAD");
		break;

	case PE_CTRL_REGISTER:
		_uReturnValue = m_Control.Hex;
		INFO_LOG(PIXELENGINE, "(r16) CTRL_REGISTER : %04x", _uReturnValue);
		break;

	case PE_TOKEN_REG:
		_uReturnValue = Common::AtomicLoad(*(volatile u32*)&CommandProcessor::fifo.PEToken);
		INFO_LOG(PIXELENGINE, "(r16) TOKEN_REG : %04x", _uReturnValue);
		break;

	case PE_BBOX_LEFT:
	{
		// Left must be even and 606px max
		_uReturnValue = std::min((u16) 606, bbox[0]) & ~1;

		INFO_LOG(PIXELENGINE, "R: BBOX_LEFT   = %i", _uReturnValue);
		bbox_active = false;
		break;
	}

	case PE_BBOX_RIGHT:
	{
		// Right must be odd and 607px max
		_uReturnValue = std::min((u16) 607, bbox[1]) | 1;

		INFO_LOG(PIXELENGINE, "R: BBOX_RIGHT  = %i", _uReturnValue);
		bbox_active = false;
		break;
	}

	case PE_BBOX_TOP:
	{
		// Top must be even and 478px max
		_uReturnValue = std::min((u16) 478, bbox[2]) & ~1;

		INFO_LOG(PIXELENGINE, "R: BBOX_TOP    = %i", _uReturnValue);
		bbox_active = false;
		break;
	}

	case PE_BBOX_BOTTOM:
	{
		// Bottom must be odd and 479px max
		_uReturnValue = std::min((u16) 479, bbox[3]) | 1;

		INFO_LOG(PIXELENGINE, "R: BBOX_BOTTOM = %i", _uReturnValue);
		bbox_active = false;
		break;
	}

	// NOTE(neobrain): only PE_PERF_ZCOMP_OUTPUT is implemented in D3D11, but the other values shouldn't be contradictionary to the value of that register (i.e. INPUT registers should always be greater or equal to their corresponding OUTPUT registers).
	case PE_PERF_ZCOMP_INPUT_ZCOMPLOC_L:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_INPUT_ZCOMPLOC) & 0xFFFF;
		break;

	case PE_PERF_ZCOMP_INPUT_ZCOMPLOC_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_INPUT_ZCOMPLOC) >> 16;
		break;

	case PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_L:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_OUTPUT_ZCOMPLOC) & 0xFFFF;
		break;

	case PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_OUTPUT_ZCOMPLOC) >> 16;
		break;

	case PE_PERF_ZCOMP_INPUT_L:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_INPUT) & 0xFFFF;
		break;

	case PE_PERF_ZCOMP_INPUT_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_INPUT) >> 16;
		break;

	case PE_PERF_ZCOMP_OUTPUT_L:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_OUTPUT) & 0xFFFF;
		break;

	case PE_PERF_ZCOMP_OUTPUT_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_ZCOMP_OUTPUT) >> 16;
		break;

	case PE_PERF_BLEND_INPUT_L:
		// Super Mario Sunshine uses this register in episode 6 of Sirena Beach:
		// The amount of remaining goop is determined by checking how many pixels reach the blending stage.
		// Once this register falls below a particular value (around 0x90), the game regards the challenge finished.
		// In very old builds, Dolphin only returned 0. That caused the challenge to be immediately finished without any goop being cleaned (the timer just didn't even start counting from 3:00:00).
		// Later builds returned 1 for the high register. That caused the timer to actually count down, but made the challenge unbeatable because the game always thought you didn't clear any goop at all.
		// Note that currently this functionality is only implemented in the D3D11 backend.
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_BLEND_INPUT) & 0xFFFF;
		break;

	case PE_PERF_BLEND_INPUT_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_BLEND_INPUT) >> 16;
		break;

	case PE_PERF_EFB_COPY_CLOCKS_L:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_EFB_COPY_CLOCKS) & 0xFFFF;
		break;

	case PE_PERF_EFB_COPY_CLOCKS_H:
		_uReturnValue = g_video_backend->Video_GetQueryResult(PQ_EFB_COPY_CLOCKS) >> 16;
		break;

	default:
		INFO_LOG(PIXELENGINE, "(r16) unknown @ %08x", _iAddress);
		_uReturnValue = 1;
		break;
	}
	
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFFF)
	{
		// CPU Direct Access EFB Raster State Config
	case PE_ZCONF:
		m_ZConf.Hex = _iValue;
		INFO_LOG(PIXELENGINE, "(w16) ZCONF: %02x", _iValue);
		break;
	case PE_ALPHACONF:
		m_AlphaConf.Hex = _iValue;
		INFO_LOG(PIXELENGINE, "(w16) ALPHACONF: %02x", _iValue);
		break;
	case PE_DSTALPHACONF:
		m_DstAlphaConf.Hex = _iValue;
		INFO_LOG(PIXELENGINE, "(w16) DSTALPHACONF: %02x", _iValue);
		break;
	case PE_ALPHAMODE:
		m_AlphaModeConf.Hex = _iValue;
		INFO_LOG(PIXELENGINE, "(w16) ALPHAMODE: %02x", _iValue);
		break;
	case PE_ALPHAREAD:
		m_AlphaRead.Hex = _iValue;
		INFO_LOG(PIXELENGINE, "(w16) ALPHAREAD: %02x", _iValue);
		break;

	case PE_CTRL_REGISTER:	
		{
			UPECtrlReg tmpCtrl(_iValue);

			if (tmpCtrl.PEToken)	g_bSignalTokenInterrupt = 0;
			if (tmpCtrl.PEFinish)	g_bSignalFinishInterrupt = 0;

			m_Control.PETokenEnable  = tmpCtrl.PETokenEnable;
			m_Control.PEFinishEnable = tmpCtrl.PEFinishEnable;
			m_Control.PEToken = 0;		// this flag is write only
			m_Control.PEFinish = 0;		// this flag is write only

			DEBUG_LOG(PIXELENGINE, "(w16) CTRL_REGISTER: 0x%04x", _iValue);
			UpdateInterrupts();
		}
		break;

	case PE_TOKEN_REG:
		PanicAlert("(w16) WTF? PowerPC program wrote token: %i", _iValue);
		//only the gx pipeline is supposed to be able to write here
		//g_token = _iValue;
		break;

	default:
		WARN_LOG(PIXELENGINE, "(w16) unknown %04x @ %08x", _iValue, _iAddress);
		break;
	}

}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	WARN_LOG(PIXELENGINE, "(w32) 0x%08x @ 0x%08x IGNORING...",_iValue,_iAddress);
}

bool AllowIdleSkipping()
{
	return !SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread || (!m_Control.PETokenEnable && !m_Control.PEFinishEnable);
}

void UpdateInterrupts()
{
	// check if there is a token-interrupt
	UpdateTokenInterrupt((g_bSignalTokenInterrupt & m_Control.PETokenEnable));
	
	// check if there is a finish-interrupt
	UpdateFinishInterrupt((g_bSignalFinishInterrupt & m_Control.PEFinishEnable));
}

void UpdateTokenInterrupt(bool active)
{
	ProcessorInterface::SetInterrupt(INT_CAUSE_PE_TOKEN, active);
	Common::AtomicStore(interruptSetToken, active ? 1 : 0);
}

void UpdateFinishInterrupt(bool active)
{
	ProcessorInterface::SetInterrupt(INT_CAUSE_PE_FINISH, active);
	Common::AtomicStore(interruptSetFinish, active ? 1 : 0);
}

// TODO(mb2): Refactor SetTokenINT_OnMainThread(u64 userdata, int cyclesLate).
//			  Think about the right order between tokenVal and tokenINT... one day maybe.
//			  Cleanup++

// Called only if BPMEM_PE_TOKEN_INT_ID is ack by GP
void SetToken_OnMainThread(u64 userdata, int cyclesLate)
{
	// XXX: No 16-bit atomic store available, so cheat and use 32-bit.
	// That's what we've always done. We're counting on fifo.PEToken to be
	// 4-byte padded.
	Common::AtomicStore(*(volatile u32*)&CommandProcessor::fifo.PEToken, userdata & 0xffff);
	INFO_LOG(PIXELENGINE, "VIDEO Backend raises INT_CAUSE_PE_TOKEN (btw, token: %04x)", CommandProcessor::fifo.PEToken);
	if (userdata >> 16)
	{
		Common::AtomicStore(*(volatile u32*)&g_bSignalTokenInterrupt, 1);
		UpdateInterrupts();
	}
	CommandProcessor::interruptTokenWaiting = false;
	IncrementCheckContextId();
}

void SetFinish_OnMainThread(u64 userdata, int cyclesLate)
{
	Common::AtomicStore(*(volatile u32*)&g_bSignalFinishInterrupt, 1);
	UpdateInterrupts();
	CommandProcessor::interruptFinishWaiting = false;
	CommandProcessor::isPossibleWaitingSetDrawDone = false;
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void SetToken(const u16 _token, const int _bSetTokenAcknowledge)
{
	if (_bSetTokenAcknowledge) // set token INT
	{
		Common::AtomicStore(*(volatile u32*)&g_bSignalTokenInterrupt, 1);
	}

	CommandProcessor::interruptTokenWaiting = true;
	CoreTiming::ScheduleEvent_Threadsafe(0, et_SetTokenOnMainThread, _token | (_bSetTokenAcknowledge << 16));
	IncrementCheckContextId();
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD (BPStructs.cpp) when a new frame has been drawn
void SetFinish()
{
	CommandProcessor::interruptFinishWaiting = true;
	CoreTiming::ScheduleEvent_Threadsafe(0, et_SetFinishOnMainThread, 0);
	INFO_LOG(PIXELENGINE, "VIDEO Set Finish");
	IncrementCheckContextId();
}

//This function is used in CommandProcessor when write CTRL_REGISTER and the new fifo is attached.
void ResetSetFinish()
{
	//if SetFinish happened but PE_CTRL_REGISTER not, I reset the interrupt else
	//remove event from the queue
	if (g_bSignalFinishInterrupt)
	{
		UpdateFinishInterrupt(false);
		g_bSignalFinishInterrupt = false;
	}
	else
	{
		CoreTiming::RemoveEvent(et_SetFinishOnMainThread);
	}
	CommandProcessor::interruptFinishWaiting = false;
}

void ResetSetToken()
{
	if (g_bSignalTokenInterrupt)
	{
		UpdateTokenInterrupt(false);
		g_bSignalTokenInterrupt = 0;
	}
	else
	{
		CoreTiming::RemoveEvent(et_SetTokenOnMainThread);
	}
	CommandProcessor::interruptTokenWaiting = false;
}
} // end of namespace PixelEngine
