// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _NULLSOUNDSTREAM_H_
#define _NULLSOUNDSTREAM_H_

#include "SoundStream.h"
#include "Thread.h"

#define BUF_SIZE (48000 * 4 / 32)

class NullSound : public SoundStream
{
	// playback position
	short realtimeBuffer[BUF_SIZE / sizeof(short)];

public:
	NullSound(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
	{}

	virtual ~NullSound() {}

	virtual bool Start();
	virtual void SoundLoop();
	virtual void SetVolume(int volume);
	virtual void Stop();
	virtual void Clear(bool mute);
	static bool isValid() { return true; }
	virtual bool usesMixer() const { return true; }
	virtual void Update();
};

#endif //_NULLSOUNDSTREAM_H_
