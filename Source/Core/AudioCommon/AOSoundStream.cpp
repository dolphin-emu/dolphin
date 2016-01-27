// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "AudioCommon/AOSoundStream.h"
#include "AudioCommon/Mixer.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

#if defined(HAVE_AO) && HAVE_AO

void AOSound::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - ao");

	const uint_32 numBytesToMix = 512;
	const uint_32 numBytesToOutput = numBytesToMix / 2;
	ao_initialize();
	default_driver = ao_default_driver_id();
	format.bits = 16;
	format.channels = 2;
	format.rate = m_mixer->GetSampleRate();
	format.byte_format = AO_FMT_LITTLE;

	device = ao_open_live(default_driver, &format, nullptr /* no options */);
	if (!device)
	{
		PanicAlertT("AudioCommon: Error opening AO device.\n");
		ao_shutdown();
		Stop();
		return;
	}

	// is buf_size being used?
	buf_size = format.bits/8 * format.channels * format.rate;

	while (m_run_thread.load())
	{
		m_mixer->Mix(realtimeBuffer, numBytesToMix / (sizeof(float) * 2));

		// Convert the samples from float to short
		s16 dest[MAX_AO_BUFFER];
		for (u32 i = 0; i < numBytesToMix / sizeof(float); ++i)
		{
			dest[i] = CMixer::FloatToSigned16(realtimeBuffer[i]);
		}

		{
		std::lock_guard<std::mutex> lk(soundCriticalSection);
		ao_play(device, (char*)dest, numBytesToOutput);
		}

		soundSyncEvent.Wait();
	}
}

bool AOSound::Start()
{
	m_run_thread.store(true);
	memset(realtimeBuffer, 0, sizeof(realtimeBuffer));

	thread = std::thread(&AOSound::SoundLoop, this);
	return true;
}

void AOSound::Update()
{
	soundSyncEvent.Set();
}

void AOSound::Stop()
{
	m_run_thread.store(false);
	soundSyncEvent.Set();

	{
	std::lock_guard<std::mutex> lk(soundCriticalSection);
	thread.join();

	if (device)
		ao_close(device);

	ao_shutdown();

	device = nullptr;
	}
}

#endif
