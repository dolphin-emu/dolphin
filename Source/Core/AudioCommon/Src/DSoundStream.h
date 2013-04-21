// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSOUNDSTREAM_H_
#define _DSOUNDSTREAM_H_

#include "SoundStream.h"

#ifdef _WIN32
#include <mmsystem.h>
#include <dsound.h>

#define BUFSIZE (1024 * 8 * 4)
#endif

class DSoundStream: public CBaseSoundStream
{
#ifdef _WIN32
public:
	DSoundStream(CMixer *mixer, void *hWnd = NULL);
	virtual ~DSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual void OnSetVolume(u32 volume) override;

	virtual void OnUpdate() override;
	virtual void OnFlushBuffers(bool mute) override;

	virtual bool OnPreThreadStart() override;
	virtual void SoundLoop() override;
	virtual void OnPreThreadJoin() override;
	virtual void OnPostThreadJoin() override;

private:
	bool CreateBuffer();
	bool WriteDataToBuffer(DWORD dwOffset, char* soundData, DWORD dwSoundBytes);
	
	static inline u32 ConvertVolume(u32 volume)
	{
		// This is in "dBA attenuation" from 0 to -10000, logarithmic
		return (u32)floor(log10((float)volume) * 5000.0f) - 10000;
	}

	static inline int FIX128(int x)
	{
		return x & (~127);
	}

	inline int ModBufferSize(int x)
	{
		return (x + m_bufferSize) % m_bufferSize;
	}

private:
	Common::Event m_soundSyncEvent;
	void *m_hWnd;

	IDirectSound8 *m_ds;
	IDirectSoundBuffer *m_dsBuffer;

	int m_bufferSize;     //i bytes
	volatile bool m_join;

	// playback position
	int m_currentPos;
	int m_lastPos;
	short m_realtimeBuffer[BUFSIZE / sizeof(short)];

#else
public:
	DSoundStream(CMixer *mixer, void *hWnd = NULL):
		CBaseSoundStream(mixer)
	{
	}
#endif // _WIN32
};

#endif //_DSOUNDSTREAM_H_
