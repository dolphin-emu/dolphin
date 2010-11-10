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

#include "AudioCommon.h"
#include "XAudio2Stream.h"

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
	IXAudio2SourceVoice* pSourceVoice;
	CMixer *m_mixer;
	Common::EventEx *soundSyncEvent;
	short *xaBuffer;

	StreamingVoiceContext(IXAudio2 *pXAudio2, CMixer *pMixer, Common::EventEx *pSyncEvent)
	{

		m_mixer = pMixer;
		soundSyncEvent = pSyncEvent;

		WAVEFORMATEXTENSIBLE wfx;

		memset(&wfx, 0, sizeof(WAVEFORMATEXTENSIBLE));
		wfx.Format.wFormatTag		= WAVE_FORMAT_EXTENSIBLE;
		wfx.Format.nSamplesPerSec	= m_mixer->GetSampleRate();
		wfx.Format.nChannels		= 2;
		wfx.Format.wBitsPerSample	= 16;
		wfx.Format.nBlockAlign		= wfx.Format.nChannels*wfx.Format.wBitsPerSample/8;
		wfx.Format.nAvgBytesPerSec	= wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
		wfx.Format.cbSize			= sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
		wfx.Samples.wValidBitsPerSample = 16;
		wfx.dwChannelMask			= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		wfx.SubFormat				= KSDATAFORMAT_SUBTYPE_PCM;

		// create source voice
		HRESULT hr;
		if(FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx, XAUDIO2_VOICE_NOSRC, 1.0f, this)))
			PanicAlert("XAudio2 CreateSourceVoice failed: %#X", hr); 

		pSourceVoice->FlushSourceBuffers();
		pSourceVoice->Start();

		xaBuffer = new s16[NUM_BUFFERS * BUFFER_SIZE];
		memset(xaBuffer, 0, NUM_BUFFERS * BUFFER_SIZE_BYTES);

		//start buffers with silence
		for(int i=0; i < NUM_BUFFERS; i++)
		{
			XAUDIO2_BUFFER buf = {0};
			buf.AudioBytes = BUFFER_SIZE_BYTES;
			buf.pAudioData = (BYTE *) &xaBuffer[i * BUFFER_SIZE];
			buf.pContext   = (void *) buf.pAudioData;
			
			pSourceVoice->SubmitSourceBuffer(&buf);
		}

	}
	
    ~StreamingVoiceContext()
    {
        IXAudio2SourceVoice* temp = pSourceVoice;
        pSourceVoice = NULL;
        temp->FlushSourceBuffers();
        temp->DestroyVoice();
        safe_delete_array(xaBuffer);
    }
	
	void StreamingVoiceContext::Stop() {
		if (pSourceVoice)
			pSourceVoice->Stop();
	}

	void StreamingVoiceContext::Play() {
		if (pSourceVoice)
			pSourceVoice->Start();
	}
	
	STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) {}
	STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
	STDMETHOD_(void, OnBufferStart) (void*) {}
	STDMETHOD_(void, OnLoopEnd) (void*) {}   
	STDMETHOD_(void, OnStreamEnd) () {}
    STDMETHOD_(void, OnBufferEnd) (void* context)
    {	//
		//  buffer end callback; gets SAMPLES_PER_BUFFER samples for a new buffer
		//
		if( !pSourceVoice || !context) return;
	
		//soundSyncEvent->Init();
		//soundSyncEvent->Wait(); //sync
		//soundSyncEvent->Spin(); //or tight sync
		
		//if (!pSourceVoice) return;

		m_mixer->Mix((short *)context, SAMPLES_PER_BUFFER);


		XAUDIO2_BUFFER buf = {0};
		buf.AudioBytes  = BUFFER_SIZE_BYTES;
		buf.pAudioData  = (byte*)context;
		buf.pContext    = context;

		pSourceVoice->SubmitSourceBuffer(&buf);
    }
};


StreamingVoiceContext* pVoiceContext = 0;

bool XAudio2::Start()
{
	//soundSyncEvent.Init();
	
    // XAudio2 init
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
	HRESULT hr;
    if(FAILED(hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_ANY_PROCESSOR))) //callback dosent seem to run on a speecific cpu anyways
    {
        PanicAlert("XAudio2 init failed: %#X", hr);
        CoUninitialize();
		return false;
    }

    // XAudio2 master voice
	// XAUDIO2_DEFAULT_CHANNELS instead of 2 for expansion?
    if(FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice, 2, m_mixer->GetSampleRate())))
    {
        PanicAlert("XAudio2 master voice creation failed: %#X", hr);
        safe_release(pXAudio2);
        CoUninitialize();
		return false;
    }

	// Volume
	if (pMasteringVoice)
		pMasteringVoice->SetVolume(m_volume);

	if (pXAudio2)
		pVoiceContext = new StreamingVoiceContext(pXAudio2, m_mixer, &soundSyncEvent);

	return true;
}

void XAudio2::SetVolume(int volume)
{
	//linear 1- .01
	m_volume = (float)volume / 100.0;

	if (pMasteringVoice)
		pMasteringVoice->SetVolume(m_volume);

}


//XAUDIO2_PERFORMANCE_DATA perfData;
//int xi = 0;
void XAudio2::Update()
{
	//soundSyncEvent.Set();

	//xi++;
	//if (xi == 100000) {
	//	xi = 0;
	//	pXAudio2->GetPerformanceData(&perfData);
	//	NOTICE_LOG(DSPHLE, "XAudio2 latency (samples): %i",perfData.CurrentLatencyInSamples);
	//	NOTICE_LOG(DSPHLE, "XAudio2    total glitches: %i",perfData.GlitchesSinceEngineStarted);
	//}
}

void XAudio2::Clear(bool mute)
{
	m_muted = mute;

	if (pVoiceContext)
	{
		if (m_muted)
			pVoiceContext->Stop();
		else
			pVoiceContext->Play();
	}
}

void XAudio2::Stop()
{
	//soundSyncEvent.Set();

    safe_delete(pVoiceContext);
    pVoiceContext = NULL;

	if(pMasteringVoice)
		pMasteringVoice->DestroyVoice();
	
    safe_release(pXAudio2);
	pMasteringVoice = NULL;
    CoUninitialize();
	//soundSyncEvent.Shutdown();
}
