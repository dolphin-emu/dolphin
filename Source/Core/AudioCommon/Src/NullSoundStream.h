// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _NULLSOUNDSTREAM_H_
#define _NULLSOUNDSTREAM_H_

#include "SoundStream.h"

#define BUF_SIZE (48000 * 4 / 32)

class NullSoundStream: public CBaseSoundStream
{
public:
	NullSoundStream(CMixer *mixer, void *hWnd = NULL);
	virtual ~NullSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual void OnUpdate() override;

private:
	// playback position
	short m_realtimeBuffer[BUF_SIZE / sizeof(short)];
};

#endif //_NULLSOUNDSTREAM_H_
