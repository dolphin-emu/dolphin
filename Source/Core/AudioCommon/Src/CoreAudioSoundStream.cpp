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

typedef struct internal
{
	AudioUnit audioUnit;
	short realtimeBuffer[1024 * 1024];	
};


OSStatus callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {

	internal *soundStruct = (internal *)inRefCon;


	for (int i=0; i < (int)ioData->mNumberBuffers; i++)
	{
        	memcpy(ioData->mBuffers[i].mData, &soundStruct->realtimeBuffer, ioData->mBuffers[i].mDataByteSize);
	
	}
	return 0;
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
	ComponentDescription desc;
	OSStatus err;
	UInt32 enableIO;
	AURenderCallbackStruct callback_struct;
	AudioStreamBasicDescription format;
	UInt32 numBytesToRender = 512;
	
	internal *soundStruct = (internal *)malloc(sizeof(internal));
	memset(soundStruct->realtimeBuffer, 0, sizeof(soundStruct->realtimeBuffer));
	
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
  	desc.componentFlags = 0;
  	desc.componentFlagsMask = 0;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	Component component = FindNextComponent(NULL, &desc);
	if (component == NULL)
		printf("error finding component\n");	
	
	err = OpenAComponent(component, &soundStruct->audioUnit);
	if (err)
		printf("error opening audio component\n");

	//enable output device
	enableIO = 1;
	AudioUnitSetProperty(soundStruct->audioUnit,
						 kAudioOutputUnitProperty_EnableIO,
						 kAudioUnitScope_Output,
						 0,
						 &enableIO,
						 sizeof(enableIO));
	
	
	format.mBitsPerChannel = 16;
	format.mChannelsPerFrame = 2;
	format.mBytesPerPacket = 4;
	format.mBytesPerFrame = 4;
	format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFramesPerPacket = 1;
	format.mSampleRate = m_mixer->GetSampleRate();
	
	//set format to output scope
    	err = AudioUnitSetProperty(soundStruct->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(AudioStreamBasicDescription));
	if (err)
		printf("error when setting output format\n");

	
	callback_struct.inputProc = callback;
	callback_struct.inputProcRefCon = soundStruct;
	
	err = AudioUnitSetProperty(soundStruct->audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback_struct, sizeof(callback_struct));
	if (err)
		printf("error when setting input callback\n");

	err = AudioUnitInitialize(soundStruct->audioUnit);
	if (err)
		printf("error when initiaising audiounit\n");
	
	err = AudioOutputUnitStart(soundStruct->audioUnit);
	if (err)
		printf("error when stating audiounit\n");

	while(!threadData)
	{
		soundCriticalSection.Enter();	
		m_mixer->Mix(soundStruct->realtimeBuffer, numBytesToRender);
		soundCriticalSection.Leave();
		soundSyncEvent.Wait();
	}
	return true;
	
}

void *coreAudioThread(void *args)
{

        ((CoreAudioSound *)args)->SoundLoop();
        return NULL;
}


bool CoreAudioSound::Start()
{

        soundSyncEvent.Init();
        thread = new Common::Thread(coreAudioThread, (void *)this);
	return true;
}


void CoreAudioSound::Stop()
{
    	delete thread;
    	thread = NULL;

	return;
	
}
void CoreAudioSound::Update()
{
	soundSyncEvent.Set();
	
}
