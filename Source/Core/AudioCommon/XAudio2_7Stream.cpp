// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Note that this file *and this file only* must include XAudio2.h from the old
// DXSDK instead of other possible places.

#include <XAudio2_7/XAudio2.h>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/XAudio2_7Stream.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

struct StreamingVoiceContext2_7 : public IXAudio2VoiceCallback
{
private:
  Mixer* const m_mixer;
  Common::Event& m_sound_sync_event;
  IXAudio2SourceVoice* m_source_voice;
  std::unique_ptr<BYTE[]> xaudio_buffer;

  void SubmitBuffer(PBYTE buf_data);

public:
  StreamingVoiceContext2_7(IXAudio2* pXAudio2, Mixer* pMixer, Common::Event& pSyncEvent);
  virtual ~StreamingVoiceContext2_7();

  void Stop();
  void Play();

  STDMETHOD_(void, OnVoiceError)(THIS_ void* pBufferContext, HRESULT Error) {}
  STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) {}
  STDMETHOD_(void, OnVoiceProcessingPassEnd)() {}
  STDMETHOD_(void, OnBufferStart)(void*) {}
  STDMETHOD_(void, OnLoopEnd)(void*) {}
  STDMETHOD_(void, OnStreamEnd)() {}
  STDMETHOD_(void, OnBufferEnd)(void* context);
};

const int NUM_BUFFERS = 3;
const int SAMPLES_PER_BUFFER = 96;

const int NUM_CHANNELS = 2;
const int BUFFER_SIZE = SAMPLES_PER_BUFFER * NUM_CHANNELS;
const int BUFFER_SIZE_BYTES = BUFFER_SIZE * sizeof(s16);

void StreamingVoiceContext2_7::SubmitBuffer(PBYTE buf_data)
{
  XAUDIO2_BUFFER buf = {};
  buf.AudioBytes = BUFFER_SIZE_BYTES;
  buf.pContext = buf_data;
  buf.pAudioData = buf_data;

  m_source_voice->SubmitSourceBuffer(&buf);
}

StreamingVoiceContext2_7::StreamingVoiceContext2_7(IXAudio2* pXAudio2, Mixer* pMixer,
                                                   Common::Event& pSyncEvent)
    : m_mixer(pMixer), m_sound_sync_event(pSyncEvent),
      xaudio_buffer(new BYTE[NUM_BUFFERS * BUFFER_SIZE_BYTES]())
{
  WAVEFORMATEXTENSIBLE wfx = {};

  wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfx.Format.nSamplesPerSec = m_mixer->GetSampleRate();
  wfx.Format.nChannels = 2;
  wfx.Format.wBitsPerSample = 16;
  wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
  wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
  wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  wfx.Samples.wValidBitsPerSample = 16;
  wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  // create source voice
  HRESULT hr;
  if (FAILED(hr = pXAudio2->CreateSourceVoice(&m_source_voice, &wfx.Format, XAUDIO2_VOICE_NOSRC,
                                              1.0f, this)))
  {
    PanicAlert("XAudio2_7 CreateSourceVoice failed: %#X", hr);
    return;
  }

  m_source_voice->Start();

  // start buffers with silence
  for (int i = 0; i != NUM_BUFFERS; ++i)
    SubmitBuffer(xaudio_buffer.get() + (i * BUFFER_SIZE_BYTES));
}

StreamingVoiceContext2_7::~StreamingVoiceContext2_7()
{
  if (m_source_voice)
  {
    m_source_voice->Stop();
    m_source_voice->DestroyVoice();
  }
}

void StreamingVoiceContext2_7::Stop()
{
  if (m_source_voice)
    m_source_voice->Stop();
}

void StreamingVoiceContext2_7::Play()
{
  if (m_source_voice)
    m_source_voice->Start();
}

void StreamingVoiceContext2_7::OnBufferEnd(void* context)
{
  //  buffer end callback; gets SAMPLES_PER_BUFFER samples for a new buffer

  if (!m_source_voice || !context)
    return;

  // m_sound_sync_event->Wait(); // sync
  // m_sound_sync_event->Spin(); // or tight sync

  m_mixer->Mix(static_cast<short*>(context), SAMPLES_PER_BUFFER);
  SubmitBuffer(static_cast<BYTE*>(context));
}

HMODULE XAudio2_7::m_xaudio2_dll = nullptr;

void XAudio2_7::ReleaseIXAudio2(IXAudio2* ptr)
{
  ptr->Release();
}

bool XAudio2_7::InitLibrary()
{
  if (m_xaudio2_dll)
  {
    return true;
  }

  m_xaudio2_dll = ::LoadLibrary(TEXT("xaudio2_7.dll"));

  return m_xaudio2_dll != nullptr;
}

XAudio2_7::XAudio2_7()
    : m_mastering_voice(nullptr), m_volume(1.0f),
      m_cleanup_com(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
{
}

XAudio2_7::~XAudio2_7()
{
  Stop();
  if (m_cleanup_com)
    CoUninitialize();
}

bool XAudio2_7::Start()
{
  HRESULT hr;

  // callback doesn't seem to run on a specific CPU anyways
  IXAudio2* xaudptr;
  if (FAILED(hr = XAudio2Create(&xaudptr, 0, XAUDIO2_DEFAULT_PROCESSOR)))
  {
    PanicAlert("XAudio2_7 init failed: %#X", hr);
    Stop();
    return false;
  }
  m_xaudio2 = std::unique_ptr<IXAudio2, Releaser>(xaudptr);

  // XAudio2 master voice
  // XAUDIO2_DEFAULT_CHANNELS instead of 2 for expansion?
  if (FAILED(hr = m_xaudio2->CreateMasteringVoice(&m_mastering_voice, 2, m_mixer->GetSampleRate())))
  {
    PanicAlert("XAudio2_7 master voice creation failed: %#X", hr);
    Stop();
    return false;
  }

  // Volume
  m_mastering_voice->SetVolume(m_volume);

  m_voice_context = std::unique_ptr<StreamingVoiceContext2_7>(
      new StreamingVoiceContext2_7(m_xaudio2.get(), m_mixer.get(), m_sound_sync_event));

  return true;
}

void XAudio2_7::SetVolume(int volume)
{
  // linear 1- .01
  m_volume = (float)volume / 100.f;

  if (m_mastering_voice)
    m_mastering_voice->SetVolume(m_volume);
}

void XAudio2_7::Clear(bool mute)
{
  m_muted = mute;

  if (m_voice_context)
  {
    if (m_muted)
      m_voice_context->Stop();
    else
      m_voice_context->Play();
  }
}

void XAudio2_7::Stop()
{
  // m_sound_sync_event.Set();

  m_voice_context.reset();

  if (m_mastering_voice)
  {
    m_mastering_voice->DestroyVoice();
    m_mastering_voice = nullptr;
  }

  m_xaudio2.reset();  // release interface

  if (m_xaudio2_dll)
  {
    ::FreeLibrary(m_xaudio2_dll);
    m_xaudio2_dll = nullptr;
  }
}
