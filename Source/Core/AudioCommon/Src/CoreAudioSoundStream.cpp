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

    	UInt32 remaining, len;
    	void *ptr;
    	AudioBuffer *data;

	internal *soundStruct = (internal *)inRefCon;

	data = &ioData->mBuffers[0];
    	len = data->mDataByteSize;
    	ptr = data->mData;

	memcpy(ptr, soundStruct->realtimeBuffer, len);
	
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
	UInt32 shouldAllocateBuffer = 1;
	AudioStreamBasicDescription format;
	
	internal *soundStruct = (internal *)malloc(sizeof(internal));

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
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
	
	AudioUnitSetProperty(soundStruct->audioUnit, kAudioUnitProperty_ShouldAllocateBuffer, kAudioUnitScope_Global, 1, &shouldAllocateBuffer, sizeof(shouldAllocateBuffer));
	if (err)
		printf("error while allocate audiounit buffer\n");
	
	
	format.mBitsPerChannel = 16;
	format.mChannelsPerFrame = 2;
	format.mBytesPerPacket = sizeof(float);
	format.mBytesPerFrame = sizeof(float);
	format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFramesPerPacket = 1;
	format.mSampleRate = m_mixer->GetSampleRate();
	
	//set format to output scope
    	err = AudioUnitSetProperty(soundStruct->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &format, sizeof(AudioStreamBasicDescription));
	if (err)
		printf("error when setting output format\n");

	
	callback_struct.inputProc = callback;
	callback_struct.inputProcRefCon = soundStruct;
	
	err = AudioUnitSetProperty(soundStruct->audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &callback_struct, sizeof(callback_struct));
	if (err)
		printf("error when setting output callback\n");

	err = AudioUnitInitialize(soundStruct->audioUnit);
	if (err)
		printf("error when initiaising audiounit\n");
	
	err = AudioOutputUnitStart(soundStruct->audioUnit);
	if (err)
		printf("error when stating audiounit\n");

	while(!threadData)
	{
		m_mixer->Mix(soundStruct->realtimeBuffer, 2048);
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
	return;
	
}
