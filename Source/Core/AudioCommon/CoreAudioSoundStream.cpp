// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <CoreServices/CoreServices.h>

#include "AudioCommon/CoreAudioSoundStream.h"

OSStatus CoreAudioSound::callback(void *inRefCon,
	AudioUnitRenderActionFlags *ioActionFlags,
	const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
	UInt32 inNumberFrames, AudioBufferList *ioData)
{
	for (UInt32 i = 0; i < ioData->mNumberBuffers; i++)
		((CoreAudioSound *)inRefCon)->m_mixer->
			Mix((short *)ioData->mBuffers[i].mData,
				ioData->mBuffers[i].mDataByteSize / 4);

	return noErr;
}

CoreAudioSound::CoreAudioSound(CMixer *mixer) : SoundStream(mixer)
{
}

CoreAudioSound::~CoreAudioSound()
{
}

bool CoreAudioSound::Start()
{
	OSStatus err;
	AURenderCallbackStruct callback_struct;
	AudioStreamBasicDescription format;
	AudioComponentDescription desc;
	AudioComponent component;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	component = AudioComponentFindNext(nullptr, &desc);
	if (component == nullptr)
	{
		ERROR_LOG(AUDIO, "error finding audio component");
		return false;
	}

	err = AudioComponentInstanceNew(component, &audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error opening audio component");
		return false;
	}

	FillOutASBDForLPCM(format, m_mixer->GetSampleRate(),
				2, 16, 16, false, false, false);
	err = AudioUnitSetProperty(audioUnit,
				kAudioUnitProperty_StreamFormat,
				kAudioUnitScope_Input, 0, &format,
				sizeof(AudioStreamBasicDescription));
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting audio format");
		return false;
	}

	callback_struct.inputProc = callback;
	callback_struct.inputProcRefCon = this;
	err = AudioUnitSetProperty(audioUnit,
				kAudioUnitProperty_SetRenderCallback,
				kAudioUnitScope_Input, 0, &callback_struct,
				sizeof callback_struct);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting audio callback");
		return false;
	}

	err = AudioUnitSetParameter(audioUnit,
					kHALOutputParam_Volume,
					kAudioUnitParameterFlag_Output, 0,
					m_volume / 100., 0);
	if (err != noErr)
		ERROR_LOG(AUDIO, "error setting volume");

	err = AudioUnitInitialize(audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error initializing audiounit");
		return false;
	}

	err = AudioOutputUnitStart(audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error starting audiounit");
		return false;
	}

	return true;
}

void CoreAudioSound::SetVolume(int volume)
{
	OSStatus err;
	m_volume = volume;

	err = AudioUnitSetParameter(audioUnit,
					kHALOutputParam_Volume,
					kAudioUnitParameterFlag_Output, 0,
					volume / 100., 0);
	if (err != noErr)
		ERROR_LOG(AUDIO, "error setting volume");
}

void CoreAudioSound::SoundLoop()
{
}

void CoreAudioSound::Stop()
{
	OSStatus err;

	err = AudioOutputUnitStop(audioUnit);
	if (err != noErr)
		ERROR_LOG(AUDIO, "error stopping audiounit");

	err = AudioUnitUninitialize(audioUnit);
	if (err != noErr)
		ERROR_LOG(AUDIO, "error uninitializing audiounit");

	err = AudioComponentInstanceDispose(audioUnit);
	if (err != noErr)
		ERROR_LOG(AUDIO, "error closing audio component");
}

void CoreAudioSound::Update()
{
}
