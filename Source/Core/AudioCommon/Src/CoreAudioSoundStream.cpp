// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "CoreAudioSoundStream.h"

#ifdef __APPLE__

#include <CoreServices/CoreServices.h>

CoreAudioSound::CoreAudioSound(CMixer *mixer):
	CBaseSoundStream(mixer),
	m_audioUnit(NULL)
{
}

CoreAudioSound::~CoreAudioSound()
{
}

void CoreAudioSound::OnSetVolume(u32 volume)
{
	OSStatus err = AudioUnitSetParameter(m_audioUnit,
					kHALOutputParam_Volume,
					kAudioUnitParameterFlag_Output, 0,
					ConvertVolume(volume), 0);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting volume\n");
	}
}

bool CoreAudioSound::OnPreThreadStart()
{
	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	Component component = FindNextComponent(NULL, &desc);
	if (component == NULL)
	{
		ERROR_LOG(AUDIO, "error finding audio component\n");
		return false;
	}

	OSStatus err = OpenAComponent(component, &m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error opening audio component\n");
		return false;
	}

	AudioStreamBasicDescription format;
	unsigned int sample_rate = CBaseSoundStream::GetMixer()->GetSampleRate();
	FillOutASBDForLPCM(format, sample_rate,
				2, 16, 16, false, false, false);
	err = AudioUnitSetProperty(m_audioUnit,
				kAudioUnitProperty_StreamFormat,
				kAudioUnitScope_Input, 0, &format,
				sizeof(AudioStreamBasicDescription));
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting audio format\n");
		return false;
	}

	AURenderCallbackStruct callback_struct;
	callback_struct.inputProc = &CoreAudioSound::callback;
	callback_struct.inputProcRefCon = this;
	err = AudioUnitSetProperty(m_audioUnit,
				kAudioUnitProperty_SetRenderCallback,
				kAudioUnitScope_Input, 0, &callback_struct,
				sizeof callback_struct);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting audio callback\n");
		return false;
	}

	u32 vol = CBaseSoundStream::GetVolume();
    err = AudioUnitSetParameter(m_audioUnit,
					kHALOutputParam_Volume,
					kAudioUnitParameterFlag_Output, 0,
					ConvertVolume(vol), 0);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error setting volume\n");
	}

	err = AudioUnitInitialize(m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error initializing audiounit\n");
		return false;
	}

	err = AudioOutputUnitStart(m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error starting audiounit\n");
		return false;
	}

	return true;
}

void CoreAudioSound::OnPreThreadJoin()
{
	OSStatus err = AudioOutputUnitStop(m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error stopping audiounit\n");
	}

	err = AudioUnitUninitialize(m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error uninitializing audiounit\n");
	}

	err = CloseComponent(m_audioUnit);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "error closing audio component\n");
	}	
}

AudioUnitParameterValue CoreAudioSound::ConvertVolume(u32 volume)
{
	return (AudioUnitParameterValue)volume / 100.0f;
}

OSStatus CoreAudioSound::callback(
	void *inRefCon,
	AudioUnitRenderActionFlags *ioActionFlags,
	const AudioTimeStamp *inTimeStamp,
	UInt32 inBusNumber,
	UInt32 inNumberFrames,
	AudioBufferList *ioData)
{
	CoreAudioSound *this_obj = static_cast<CoreAudioSound*>(inRefCon);
	CMixer *mixer = this_obj->GetMixer();
	for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
	{
		short *buffers = (short*)ioData->mBuffers[i].mData;
		// TODO, FIXME: shouldn't this be / 2??
		int num_samples = ioData->mBuffers[i].mDataByteSize / 4;
		mixer->Mix(buffers, num_samples);
	}

	return noErr;
}

#endif
