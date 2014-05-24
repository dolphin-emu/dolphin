// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"
#include "Common/StdThread.h"

#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define BUFSIZE (256 * 8 * 4)
#endif

class DSound final : public SoundStream
{
#ifdef _WIN32
	std::thread thread;
	HANDLE soundSyncEvent;
	void  *hWnd;

	IDirectSound8* ds;
	IDirectSoundBuffer* dsBuffer;

	int bufferSize;     //i bytes
	int m_volume;

	// playback position
	int currentPos;
	int lastPos;
	short realtimeBuffer[BUFSIZE / sizeof(short)];

	inline int FIX128(int x)
	{
		return x & (~127);
	}

	inline int ModBufferSize(int x)
	{
		return (x + bufferSize) % bufferSize;
	}

	bool CreateBuffer();
	bool WriteDataToBuffer(DWORD dwOffset, char* soundData, DWORD dwSoundBytes);

public:
	DSound(CMixer *mixer, void *_hWnd)
		: SoundStream(mixer)
		, bufferSize(0)
		, currentPos(0)
		, lastPos(0)
		, dsBuffer(0)
		, ds(0)
		, hWnd(_hWnd)
	{}

	virtual ~DSound() {}

	virtual bool Start();
	virtual void SoundLoop();
	virtual void SetVolume(int volume);
	virtual void Stop();
	virtual void Clear(bool mute);
	static bool isValid() { return true; }

#else
public:
	DSound(CMixer *mixer, void *_hWnd)
		: SoundStream(mixer)
	{}
#endif
};
