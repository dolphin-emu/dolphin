// Copyright (C) 2003-2009 Dolphin Project.

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

#include <string.h>

#include "AOSoundStream.h"

#if defined(HAVE_AO) && HAVE_AO

void AOSound::SoundLoop()
{
    ao_initialize();
    default_driver = ao_default_driver_id();
    format.bits = 16;
    format.channels = 2;
    format.rate = sampleRate;
    format.byte_format = AO_FMT_LITTLE;
		
    device = ao_open_live(default_driver, &format, NULL /* no options */);
    if (device == NULL) {
	PanicAlert("DSP_HLE: Error opening AO device.\n");
	ao_shutdown();
	Stop();
	return;
    }   

    buf_size = format.bits/8 * format.channels * format.rate;

    while (!threadData) {
            soundCriticalSection->Enter();
                    
            uint_32 numBytesToRender = 256;
            (*callback)(realtimeBuffer, numBytesToRender >> 2, 16, sampleRate, 2);
            ao_play(device, (char*)realtimeBuffer, numBytesToRender);
            soundCriticalSection->Leave();
            soundSyncEvent->Wait();
    }
}

void *soundThread(void *args) {
	((AOSound *)args)->SoundLoop();
	return NULL;
}

bool AOSound::Start() {
    memset(realtimeBuffer, 0, sizeof(realtimeBuffer));
    soundCriticalSection = new Common::CriticalSection(1);
    thread = new Common::Thread(soundThread, (void *)this);
    soundSyncEvent = new Common::Event();
    soundSyncEvent->Init();
    return true;
}

void AOSound::Update() {
    soundSyncEvent->Set();
}
    
void AOSound::Stop()
{
    soundCriticalSection->Enter();
    threadData = 1;
    soundSyncEvent->Set();
    soundCriticalSection->Leave();
    soundSyncEvent->Shutdown();
    delete soundCriticalSection;
    delete thread;
    delete soundSyncEvent;

    ao_close(device);
    device = NULL;
    ao_shutdown();
}

#endif
