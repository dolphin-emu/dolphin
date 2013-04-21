// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon.h"
#include "XAudio2Stream.h"

const int NUM_BUFFERS = 3;
const int SAMPLES_PER_BUFFER = 96;

const int NUM_CHANNELS = 2;
const int BUFFER_SIZE = SAMPLES_PER_BUFFER * NUM_CHANNELS;
const int BUFFER_SIZE_BYTES = BUFFER_SIZE * sizeof(s16);

void StreamingVoiceContext::SubmitBuffer(PBYTE buf_data)
{
	XAUDIO2_BUFFER buf = {};
	buf.AudioBytes = BUFFER_SIZE_BYTES;
	buf.pContext = buf_data;
	buf.pAudioData = buf_data;

	m_source_voice->SubmitSourceBuffer(&buf);
}

StreamingVoiceContext::StreamingVoiceContext(IXAudio2 *pXAudio2, CMixer *pMixer, Common::Event& pSyncEvent)
	: m_mixer(pMixer)
	, m_sound_sync_event(pSyncEvent)
	, xaudio_buffer(new BYTE[NUM_BUFFERS * BUFFER_SIZE_BYTES]())
{
	WAVEFORMATEXTENSIBLE wfx = {};

	wfx.Format.wFormatTag		= WAVE_FORMAT_EXTENSIBLE;
	wfx.Format.nSamplesPerSec	= m_mixer->GetSampleRate();
	wfx.Format.nChannels		= 2;
	wfx.Format.wBitsPerSample	= 16;
	wfx.Format.nBlockAlign		= wfx.Format.nChannels*wfx.Format.wBitsPerSample / 8;
	wfx.Format.nAvgBytesPerSec	= wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
	wfx.Format.cbSize			= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	wfx.Samples.wValidBitsPerSample = 16;
	wfx.dwChannelMask			= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	wfx.SubFormat				= KSDATAFORMAT_SUBTYPE_PCM;

	// create source voice
	HRESULT hr;
	if (FAILED(hr = pXAudio2->CreateSourceVoice(&m_source_voice, &wfx.Format, XAUDIO2_VOICE_NOSRC, 1.0f, this)))
	{
		PanicAlertT("XAudio2 CreateSourceVoice failed: %#X", hr);
		return;
	}

	m_source_voice->Start();

	// start buffers with silence
	for (int i = 0; i != NUM_BUFFERS; ++i)
		SubmitBuffer(xaudio_buffer.get() + (i * BUFFER_SIZE_BYTES));
}

StreamingVoiceContext::~StreamingVoiceContext()
{
	if (m_source_voice)
	{
		m_source_voice->Stop();
		m_source_voice->DestroyVoice();
	}
}

void StreamingVoiceContext::Stop()
{
	if (m_source_voice)
		m_source_voice->Stop();
}

void StreamingVoiceContext::Play()
{
	if (m_source_voice)
		m_source_voice->Start();
}

void StreamingVoiceContext::OnBufferEnd(void* context)
{
	//  buffer end callback; gets SAMPLES_PER_BUFFER samples for a new buffer

	if (!m_source_voice || !context)
		return;

	//m_sound_sync_event->Wait(); // sync
	//m_sound_sync_event->Spin(); // or tight sync

	m_mixer->Mix(static_cast<short*>(context), SAMPLES_PER_BUFFER);
	SubmitBuffer(static_cast<BYTE*>(context));
}

XAudio2SoundStream::XAudio2SoundStream(CMixer *mixer):
	CBaseSoundStream(mixer),
	m_xaudio2(nullptr),
	m_voice_context(nullptr),
	m_mastering_voice(nullptr),
	m_cleanup_com(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
{
}

XAudio2SoundStream::~XAudio2SoundStream()
{
	Stop();
	if (m_cleanup_com)
	{
		CoUninitialize();
	}
}

void XAudio2SoundStream::OnSetVolume(u32 volume)
{
	if (m_mastering_voice)
	{
		m_mastering_voice->SetVolume(ConvertVolume(volume));
	}
}

void XAudio2SoundStream::OnFlushBuffers(bool mute)
{
	if (m_voice_context)
	{
		if (mute)
		{
			m_voice_context->Stop();
		}
		else
		{
			m_voice_context->Play();
		}
	}
}

bool XAudio2SoundStream::OnPreThreadStart()
{
	HRESULT hr;

	// callback doesn't seem to run on a specific cpu anyways
	IXAudio2* xaudptr;
	if (FAILED(hr = XAudio2Create(&xaudptr, 0, XAUDIO2_DEFAULT_PROCESSOR)))
	{
		PanicAlertT("XAudio2 init failed: %#X", hr);
		Stop();
		return false;
	}
	m_xaudio2 = std::unique_ptr<IXAudio2, Releaser>(xaudptr);

	CMixer *mixer = CBaseSoundStream::GetMixer();
	// XAudio2 master voice
	// XAUDIO2_DEFAULT_CHANNELS instead of 2 for expansion?
	if (FAILED(hr = m_xaudio2->CreateMasteringVoice(&m_mastering_voice, 2, mixer->GetSampleRate())))
	{
		PanicAlertT("XAudio2 master voice creation failed: %#X", hr);
		Stop();
		return false;
	}

	// Volume
	u32 vol = CBaseSoundStream::GetVolume();
	m_mastering_voice->SetVolume(ConvertVolume(vol));

	m_voice_context = std::unique_ptr<StreamingVoiceContext>
		(new StreamingVoiceContext(m_xaudio2.get(), mixer, m_sound_sync_event));

	return true;
}

void XAudio2SoundStream::OnPreThreadJoin()
{
	Stop();
}

void XAudio2SoundStream::Stop()
{
	//m_sound_sync_event.Set();

	m_voice_context.reset();

	if (m_mastering_voice)
	{
		m_mastering_voice->DestroyVoice();
		m_mastering_voice = nullptr;
	}

	m_xaudio2.reset();	// release interface
}
