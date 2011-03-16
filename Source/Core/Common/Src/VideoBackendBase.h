// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef VIDEO_BACKEND_H_
#define VIDEO_BACKEND_H_

#include <string>
#include <vector>

#include "ChunkFile.h"

typedef void (*writeFn16)(const u16,const u32);
typedef void (*writeFn32)(const u32,const u32);
typedef void (*readFn16)(u16&, const u32);


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
	volatile u32 CPBase;
	volatile u32 CPEnd;
	u32 CPHiWatermark;
	u32 CPLoWatermark;
	volatile u32 CPReadWriteDistance;
	volatile u32 CPWritePointer;
	volatile u32 CPReadPointer;
	volatile u32 CPBreakpoint;
	volatile u32 SafeCPReadPointer;
	// Super Monkey Ball Adventure require this.
	// Because the read&check-PEToken-loop stays in its JITed block I suppose.
	// So no possiblity to ack the Token irq by the scheduler until some sort of PPC watchdog do its mess.
	volatile u16 PEToken;

	volatile u32 bFF_GPLinkEnable;
	volatile u32 bFF_GPReadEnable;
	volatile u32 bFF_BPEnable;
	volatile u32 bFF_BPInt;
	volatile u32 bFF_Breakpoint;

	volatile u32 CPCmdIdle;
	volatile u32 CPReadIdle;        

	volatile u32 bFF_LoWatermarkInt;
	volatile u32 bFF_HiWatermarkInt;

	volatile u32 bFF_LoWatermark;
	volatile u32 bFF_HiWatermark;

	// for GP watchdog hack
	volatile u32 Fake_GPWDToken; // cicular incrementer
	volatile u32 isGpuReadingData;
};

class VideoBackend
{
public:
	virtual ~VideoBackend() {}

	virtual void EmuStateChange(EMUSTATE_CHANGE) = 0;

	virtual void UpdateFPSDisplay(const char*) = 0;

	virtual unsigned int PeekMessages() = 0;

	virtual bool Initialize(void *&) = 0;
	virtual void Shutdown() = 0;

	virtual void DoState(PointerWrap &p) = 0;
	virtual void RunLoop(bool enable) = 0;

	virtual std::string GetName() = 0;

	virtual void ShowConfig(void*) {}

	virtual void Video_Prepare() = 0;
	virtual void Video_EnterLoop() = 0;
	virtual void Video_ExitLoop() = 0;

	virtual void Video_BeginField(u32, FieldType, u32, u32) = 0;
	virtual void Video_EndField() = 0;

	virtual u32 Video_AccessEFB(EFBAccessType, u32, u32, u32) = 0;

	virtual void Video_AddMessage(const char* pstr, unsigned int milliseconds) = 0;
	virtual void Video_ClearMessages() = 0;
	virtual bool Video_Screenshot(const char* filename) = 0;

	virtual void Video_SetRendering(bool bEnabled) = 0;

	virtual void Video_GatherPipeBursted() = 0;

	virtual bool Video_IsPossibleWaitingSetDrawDone() = 0;
	virtual void Video_AbortFrame() = 0;

	virtual readFn16  Video_CPRead16() = 0;
	virtual writeFn16 Video_CPWrite16() = 0;
	virtual readFn16  Video_PERead16() = 0;
	virtual writeFn16 Video_PEWrite16() = 0;
	virtual writeFn32 Video_PEWrite32() = 0;

	static void PopulateList();
	static void ClearList();
	static void ActivateBackend(const std::string& name);
};

extern std::vector<VideoBackend*> g_available_video_backends;
extern VideoBackend* g_video_backend;

// inherited by dx9/dx11/ogl backends
class VideoBackendHardware : public VideoBackend
{
	void DoState(PointerWrap &p);
	void RunLoop(bool enable);

	void EmuStateChange(EMUSTATE_CHANGE);

	void Video_EnterLoop();
	void Video_ExitLoop();
	void Video_BeginField(u32, FieldType, u32, u32);
	void Video_EndField();
	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32);

	void Video_AddMessage(const char* pstr, unsigned int milliseconds);
	void Video_ClearMessages();
	bool Video_Screenshot(const char* filename);

	void Video_SetRendering(bool bEnabled);

	void Video_GatherPipeBursted();

	bool Video_IsPossibleWaitingSetDrawDone();
	void Video_AbortFrame();

	readFn16  Video_CPRead16();
	writeFn16 Video_CPWrite16();
	readFn16  Video_PERead16();
	writeFn16 Video_PEWrite16();
	writeFn32 Video_PEWrite32();
};

#endif
