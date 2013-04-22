// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#if defined(HAVE_AO) && HAVE_AO

#include <functional>
#include <string.h>

#include "AOSoundStream.h"
#include "Mixer.h"

AOSoundStream::AOSoundStream(CMixer *mixer):
	CBaseSoundStream(mixer),
	m_buf_size(0),
	m_join(false),
	m_device(NULL),
	m_default_driver(0)
{
}

AOSoundStream::~AOSoundStream()
{
}

void AOSoundStream::OnUpdate()
{
	m_soundSyncEvent.Set();
}

bool AOSoundStream::OnPreThreadStart()
{
	memset(m_realtimeBuffer, 0, sizeof(m_realtimeBuffer));
	return true;
}

void AOSoundStream::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - ao");

	CMixer *mixer = CBaseSoundStream::GetMixer();
	
	ao_initialize();
	m_default_driver = ao_default_driver_id();
	m_format.bits = 16;
	m_format.channels = 2;
	m_format.rate = mixer->GetSampleRate();
	m_format.byte_format = AO_FMT_LITTLE;

	m_device = ao_open_live(m_default_driver, &m_format, NULL /* no options */);
	if (!m_device)
	{
		PanicAlertT("AudioCommon: Error opening AO device.\n");
		ao_shutdown();
		return;
	}

	m_buf_size = m_format.bits/8 * m_format.channels * m_format.rate;
	uint_32 numBytesToRender = 256;
	while (!m_join)
	{
		mixer->Mix(m_realtimeBuffer, numBytesToRender >> 2);

		{
			std::lock_guard<std::mutex> lk(m_soundCriticalSection);
			ao_play(m_device, (char*)m_realtimeBuffer, numBytesToRender);
		}

		m_soundSyncEvent.Wait();
	}
}

void AOSoundStream::OnPreThreadJoin()
{
	m_join = true;
	m_soundSyncEvent.Set();
}

void AOSoundStream::OnPostThreadJoin()
{
	std::lock_guard<std::mutex> lk(m_soundCriticalSection);
	if (m_device)
	{
		ao_close(m_device);
		m_device = NULL;
	}
	ao_shutdown();
}

#endif // defined(HAVE_AO) && HAVE_AO
