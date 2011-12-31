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

#include <functional>
#include <string.h>

#include "AOSoundStream.h"
#include "Mixer.h"

#if defined(HAVE_AO) && HAVE_AO

void AOSound::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - ao");

	uint_32 numBytesToRender = 256;
	ao_initialize();
	default_driver = ao_default_driver_id();
	format.bits = 16;
	format.channels = 2;
	format.rate = m_mixer->GetSampleRate();
	format.byte_format = AO_FMT_LITTLE;
	
	device = ao_open_live(default_driver, &format, NULL /* no options */);
	if (!device)
	{
		PanicAlertT("AudioCommon: Error opening AO device.\n");
		ao_shutdown();
		Stop();
		return;
	}

	buf_size = format.bits/8 * format.channels * format.rate;

	while (!threadData)
	{
		m_mixer->Mix(realtimeBuffer, numBytesToRender >> 2);
		
		{
		std::lock_guard<std::mutex> lk(soundCriticalSection);
		ao_play(device, (char*)realtimeBuffer, numBytesToRender);
		}

		soundSyncEvent.Wait();
	}
}

bool AOSound::Start()
{
	memset(realtimeBuffer, 0, sizeof(realtimeBuffer));
	
	thread = std::thread(std::mem_fun(&AOSound::SoundLoop), this);
	return true;
}

void AOSound::Update()
{
	soundSyncEvent.Set();
}

void AOSound::Stop()
{
	threadData = 1;
	soundSyncEvent.Set();

	{
	std::lock_guard<std::mutex> lk(soundCriticalSection);
	thread.join();

	if (device)
		ao_close(device);

	ao_shutdown();

	device = NULL;
	}
}

AOSound::~AOSound()
{
}

#endif
