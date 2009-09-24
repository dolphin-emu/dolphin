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

#include "CoreAudioSoundStream.h"


AudioDeviceID outputDeviceId;
AudioStreamBasicDescription  outputStreamBasicDescription;
AudioDeviceIOProcID	outputProcId;
UInt32 outputBufferByteCount;

static OSStatus outputIoProc(AudioDeviceID inDevice, const AudioTimeStamp *inNow,const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData)
{
	
}

void CoreAudioSound::SoundLoop()
{
	CoreAudioInit();
	
}
CoreAudioSound::CoreAudioSound(CMixer *mixer) : SoundStream(mixer)
{
}

CoreAudioSound::~CoreAudioSound()
{
}
bool CoreAudioSound::CoreAudioInit()
{
	
	OSStatus status;
	UInt32 size;
	size = sizeof(outputDeviceId);
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &outputDeviceId);
	if (outputDeviceId == kAudioDeviceUnknown) {
		printf("CoreAudioDevice Unknown\n");
		return false;
	}
	
	size = sizeof(outputStreamBasicDescription);
	status = AudioDeviceGetProperty(outputDeviceId, 0, false, kAudioDevicePropertyStreamFormat, &size, &outputStreamBasicDescription);
	
	size = sizeof(outputBufferByteCount);
	
	outputBufferByteCount = 8192;
	status = AudioDeviceSetProperty(outputDeviceId, 0, 0, false, kAudioDevicePropertyBufferSize, size, &outputBufferByteCount);
	
	status = AudioDeviceGetProperty(outputDeviceId, 0, false, kAudioDevicePropertyBufferSize, &size, &outputBufferByteCount);
	
	
	status = AudioDeviceCreateIOProcID( outputDeviceId,  outputIoProc, NULL, &outputProcId );
	
	return true;
	
}

bool CoreAudioSound::Start()
{
	AudioDeviceStart(outputDeviceId,outputProcId);
	return true;
}


void CoreAudioSound::Stop()
{
	return;
	
}
void CoreAudioSound::Update()
{
	return;
	
}