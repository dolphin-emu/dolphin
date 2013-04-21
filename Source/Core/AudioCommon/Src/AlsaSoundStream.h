// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
