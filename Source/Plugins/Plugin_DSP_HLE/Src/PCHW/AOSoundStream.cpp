// Copyright (C) 2003-2008 Dolphin Project.

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

#include <ao/ao.h>
#include <pthread.h>
#include <string.h>

#include "AOSoundStream.h"

namespace AOSound
{
	pthread_t thread;
	StreamCallback callback;

	int buf_size;
	
	ao_device *device;
	ao_sample_format format;
	int default_driver;
	
	int sampleRate;
	volatile int threadData;

	short realtimeBuffer[1024 * 1024];

	int AOSound_GetSampleRate()
	{
		return sampleRate;
	}

	void *soundThread(void *)
	{
		ao_initialize();
		default_driver = ao_default_driver_id();
		format.bits = 16;
		format.channels = 2;
		format.rate = sampleRate;
		format.byte_format = AO_FMT_LITTLE;
		
		device = ao_open_live(default_driver, &format, NULL /* no options */);
		if (device == NULL)
		{
			fprintf(stderr, "DSP_HLE: Error opening AO device.\n");
			return false;
		}   
		buf_size = format.bits/8 * format.channels * format.rate;

		while (!threadData)
		{                    
			uint_32 numBytesToRender = 256;
			(*callback)(realtimeBuffer, numBytesToRender >> 2, 16, sampleRate, 2);
			ao_play(device, (char*)realtimeBuffer, numBytesToRender);
		}

		ao_close(device);
		device = 0;
		ao_shutdown();

		return 0;
	}

	bool AOSound_StartSound(int _sampleRate, StreamCallback _callback)
	{
		callback   = _callback;
		threadData = 0;
		sampleRate = _sampleRate;
		memset(realtimeBuffer, 0, sizeof(realtimeBuffer));
		pthread_create(&thread, NULL, soundThread, (void *)NULL);
		return true;
	}

	void AOSound_StopSound()
	{
		threadData = 1;
		void *retval;
		pthread_join(thread, &retval);
	}
}
