// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _PULSE_AUDIO_STREAM_H
#define _PULSE_AUDIO_STREAM_H

#include "Common.h"
#include "SoundStream.h"

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

#include <vector>

class PulseAudioStream: public CBaseSoundStream
{
#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
public:
	PulseAudioStream(CMixer *mixer);
	virtual ~PulseAudioStream();

	static inline bool IsValid() { return true; }

private:
	virtual bool OnPreThreadStart() override;
	virtual void SoundLoop() override;
	virtual void OnPreThreadJoin() override;

private:
	bool PulseInit();
	void PulseShutdown();
	void Write(const void *data, size_t bytes);

private:
	pa_simple *m_pa;
	volatile bool m_join;
	std::vector<s16> m_mix_buffer;

#else
public:
	PulseAudioStream(CMixer *mixer):
		CBaseSoundStream(mixer)
	{
	}
#endif // HAVE_PULSEAUDIO
};

#endif // _PULSE_AUDIO_STREAM_H
