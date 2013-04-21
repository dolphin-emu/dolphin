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
