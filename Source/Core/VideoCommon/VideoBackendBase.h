// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "VideoCommon/PerfQueryBase.h"

namespace MMIO { class Mapping; }


enum FieldType
{
	FIELD_PROGRESSIVE = 0,
	FIELD_UPPER,
	FIELD_LOWER
};

enum EFBAccessType
{
	PEEK_Z = 0,
	POKE_Z,
	PEEK_COLOR,
	POKE_COLOR
};

struct SCPFifoStruct
{
	// fifo registers
	VOLATILE_BUG u32 CPBase;
	VOLATILE_BUG u32 CPEnd;
	u32 CPHiWatermark;
	u32 CPLoWatermark;
	VOLATILE_BUG u32 CPReadWriteDistance;
	VOLATILE_BUG u32 CPWritePointer;
	VOLATILE_BUG u32 CPReadPointer;
	VOLATILE_BUG u32 CPBreakpoint;
	VOLATILE_BUG u32 SafeCPReadPointer;
	// Super Monkey Ball Adventure require this.
	// Because the read&check-PEToken-loop stays in its JITed block I suppose.
	// So no possiblity to ack the Token irq by the scheduler until some sort of PPC watchdog do its mess.
	VOLATILE_BUG u16 PEToken;

	VOLATILE_BUG u32 bFF_GPLinkEnable;
	VOLATILE_BUG u32 bFF_GPReadEnable;
	VOLATILE_BUG u32 bFF_BPEnable;
	VOLATILE_BUG u32 bFF_BPInt;
	VOLATILE_BUG u32 bFF_Breakpoint;

	VOLATILE_BUG u32 bFF_LoWatermarkInt;
	VOLATILE_BUG u32 bFF_HiWatermarkInt;

	VOLATILE_BUG u32 bFF_LoWatermark;
	VOLATILE_BUG u32 bFF_HiWatermark;

	// for GP watchdog hack
	VOLATILE_BUG u32 isGpuReadingData;
};

class VideoBackend
{
public:
	virtual ~VideoBackend() {}

	virtual void EmuStateChange(EMUSTATE_CHANGE) = 0;

	virtual unsigned int PeekMessages() = 0;

	virtual bool Initialize(void *window_handle) = 0;
	virtual void Shutdown() = 0;
	virtual void RunLoop(bool enable) = 0;

	virtual std::string GetName() const = 0;
	virtual std::string GetDisplayName() const { return GetName(); }

	virtual void ShowConfig(void*) = 0;

	virtual void Video_Prepare() = 0;
	virtual void Video_EnterLoop() = 0;
	virtual void Video_ExitLoop() = 0;
	virtual void Video_Cleanup() = 0; // called from gl/d3d thread

	virtual void Video_BeginField(u32, u32, u32, u32) = 0;
	virtual void Video_EndField() = 0;

	virtual u32 Video_AccessEFB(EFBAccessType, u32, u32, u32) = 0;
	virtual u32 Video_GetQueryResult(PerfQueryType type) = 0;
	virtual u16 Video_GetBoundingBox(int index) = 0;

	virtual void Video_AddMessage(const std::string& msg, unsigned int milliseconds) = 0;
	virtual void Video_ClearMessages() = 0;
	virtual bool Video_Screenshot(const std::string& filename) = 0;

	virtual void Video_SetRendering(bool bEnabled) = 0;

	virtual void Video_GatherPipeBursted() = 0;

	virtual void Video_Sync() = 0;

	// Registers MMIO handlers for the CommandProcessor registers.
	virtual void RegisterCPMMIO(MMIO::Mapping* mmio, u32 base) = 0;

	static void PopulateList();
	static void ClearList();
	static void ActivateBackend(const std::string& name);

	// waits until is paused and fully idle, and acquires a lock on that state.
	// or, if doLock is false, releases a lock on that state and optionally unpauses.
	// calls must be balanced and non-recursive (once with doLock true, then once with doLock false).
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock = true) = 0;

	// the implementation needs not do synchronization logic, because calls to it are surrounded by PauseAndLock now
	virtual void DoState(PointerWrap &p) = 0;

	virtual void CheckInvalidState() = 0;

	virtual void UpdateWantDeterminism(bool want) {}
};

extern std::vector<VideoBackend*> g_available_video_backends;
extern VideoBackend* g_video_backend;

// inherited by D3D/OGL backends
class VideoBackendHardware : public VideoBackend
{
	void RunLoop(bool enable) override;

	void EmuStateChange(EMUSTATE_CHANGE) override;

	void Video_EnterLoop() override;
	void Video_ExitLoop() override;
	void Video_BeginField(u32, u32, u32, u32) override;
	void Video_EndField() override;

	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32) override;
	u32 Video_GetQueryResult(PerfQueryType type) override;
	u16 Video_GetBoundingBox(int index) override;

	void Video_AddMessage(const std::string& pstr, unsigned int milliseconds) override;
	void Video_ClearMessages() override;
	bool Video_Screenshot(const std::string& filename) override;

	void Video_SetRendering(bool bEnabled) override;

	void Video_GatherPipeBursted() override;

	void Video_Sync() override;

	void RegisterCPMMIO(MMIO::Mapping* mmio, u32 base) override;

	void PauseAndLock(bool doLock, bool unpauseOnUnlock = true) override;
	void DoState(PointerWrap &p) override;

	void UpdateWantDeterminism(bool want) override;

	bool m_invalid;

public:
	void CheckInvalidState() override;

protected:
	void InitializeShared();
	void InvalidState();
};
