// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <thread>

#if defined(HAVE_ALSA) && HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"

class AlsaSound final : public SoundStream
{
#if defined(HAVE_ALSA) && HAVE_ALSA
public:
	AlsaSound();
	virtual ~AlsaSound();

	bool Start() override;
	void SoundLoop() override;
	void Stop() override;
	void Update() override;

	static bool isValid()
	{
		return true;
	}

private:
	enum class ALSAThreadStatus
	{
		RUNNING,
		STOPPING,
		STOPPED,
	};

	bool AlsaInit();
	void AlsaShutdown();

	u8 *mix_buffer;
	std::thread thread;
	std::atomic<ALSAThreadStatus> m_thread_status;

	snd_pcm_t *handle;
	int frames_to_deliver;
#endif
};
