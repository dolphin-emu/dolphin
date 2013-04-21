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

#ifndef _ALSA_SOUND_STREAM_H
#define _ALSA_SOUND_STREAM_H

#if defined(HAVE_ALSA) && HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "Common.h"
#include "SoundStream.h"

class AlsaSoundStream : public CBaseSoundStream
{
#if defined(HAVE_ALSA) && HAVE_ALSA
public:
	AlsaSoundStream(CMixer *mixer);
	virtual ~AlsaSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual bool OnPreThreadStart() override;
	virtual void SoundLoop() override;
	virtual void OnPreThreadJoin() override;

private:
	bool AlsaInit();
	void AlsaShutdown();

private:
	std::unique_ptr<u8[]> m_mix_buffer;
	snd_pcm_t *m_handle;
	snd_pcm_uframes_t m_frames_to_deliver;
	volatile bool m_join;
#else
public:
	AlsaSoundStream(CMixer *mixer):
		CBaseSoundStream(mixer)
	{
	}
#endif
};

#endif
