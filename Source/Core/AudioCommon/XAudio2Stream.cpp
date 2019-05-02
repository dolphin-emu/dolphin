// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <variant>

#include <xaudio2.h>
#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/XAudio2Stream.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"

#ifndef XAUDIO2_DLL
#error You are building this module against the wrong version of DirectX. You probably need to remove DXSDK_DIR from your include path.
#endif

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
private:
  Mixer* const m_mixer;
  Common::Event& m_sound_sync_event;
  IXAudio2SourceVoice* m_source_voice;
  std::variant<std::unique_ptr<short[]>, std::unique_ptr<float[]>> xaudio_buffer;

  void SubmitBuffer(PBYTE buf_data);

  bool m_use_surround = SConfig::GetInstance().bDPL2Decoder;

public:
  StreamingVoiceContext(IXAudio2* pXAudio2, Mixer* pMixer, Common::Event& pSyncEvent);
  virtual ~StreamingVoiceContext();

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
const int SAMPLES_PER_BUFFER_SURROUND = 256;

const int NUM_CHANNELS = 2;
const int BUFFER_SIZE = SAMPLES_PER_BUFFER * NUM_CHANNELS;
const int BUFFER_SIZE_BYTES = BUFFER_SIZE * sizeof(s16);

const int NUM_CHANNELS_SURROUND = 6;
const int BUFFER_SIZE_SURROUND = SAMPLES_PER_BUFFER_SURROUND * NUM_CHANNELS_SURROUND;
const int BUFFER_SIZE_BYTES_SURROUND = BUFFER_SIZE_SURROUND * sizeof(float);
void StreamingVoiceContext::SubmitBuffer(PBYTE buf_data)
{
  XAUDIO2_BUFFER buf = {};
  if (m_use_surround)
  {
    buf.AudioBytes = BUFFER_SIZE_BYTES_SURROUND;
  }
  else
  {
    buf.AudioBytes = BUFFER_SIZE_BYTES;
  }
  buf.pContext = buf_data;
  buf.pAudioData = buf_data;

  m_source_voice->SubmitSourceBuffer(&buf);
}

StreamingVoiceContext::StreamingVoiceContext(IXAudio2* pXAudio2, Mixer* pMixer,
                                             Common::Event& pSyncEvent)
    : m_mixer(pMixer), m_sound_sync_event(pSyncEvent)
{
  WAVEFORMATEXTENSIBLE wfx = {};
  wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfx.Format.nSamplesPerSec = m_mixer->GetSampleRate();

  // More information about these values:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd390971(v=vs.85).aspx
  if (m_use_surround)
  {
    xaudio_buffer = std::make_unique<float[]>(NUM_BUFFERS * BUFFER_SIZE_SURROUND);

    wfx.Format.nChannels = 6;
    wfx.Format.wBitsPerSample = 32;
    wfx.Samples.wValidBitsPerSample = 32;
    wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                        SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    wfx.Format.cbSize = sizeof(wfx.Samples) + sizeof(wfx.dwChannelMask) + sizeof(wfx.SubFormat);
  }
  else
  {
    xaudio_buffer = std::make_unique<short[]>(NUM_BUFFERS * BUFFER_SIZE);

    wfx.Format.nChannels = 2;
    wfx.Format.wBitsPerSample = 16;
    wfx.Samples.wValidBitsPerSample = 16;
    wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  }

  wfx.Format.nBlockAlign = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
  wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;

  // create source voice
  HRESULT hr;
  if (FAILED(hr = pXAudio2->CreateSourceVoice(&m_source_voice, &wfx.Format, XAUDIO2_VOICE_NOSRC,
                                              1.0f, this)))
  {
    PanicAlert("XAudio2 CreateSourceVoice failed: %#X", hr);
    return;
  }

  m_source_voice->Start();

  // start buffers with silence
  if (m_use_surround)
  {
    for (int i = 0; i != NUM_BUFFERS; ++i)
    {
      SubmitBuffer(
          reinterpret_cast<BYTE*>(std::get<std::unique_ptr<float[]>>(xaudio_buffer).get()) +
          (i * BUFFER_SIZE_BYTES_SURROUND));
    }
  }
  else
  {
    for (int i = 0; i != NUM_BUFFERS; ++i)
    {
      SubmitBuffer(
          reinterpret_cast<BYTE*>(std::get<std::unique_ptr<short[]>>(xaudio_buffer).get()) +
          (i * BUFFER_SIZE_BYTES));
    }
  }
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

  // m_sound_sync_event->Wait(); // sync
  // m_sound_sync_event->Spin(); // or tight sync

  if (m_use_surround)
  {
    m_mixer->MixSurround(static_cast<float*>(context), SAMPLES_PER_BUFFER_SURROUND);
  }
  else
  {
    m_mixer->Mix(static_cast<short*>(context), SAMPLES_PER_BUFFER);
  }
  SubmitBuffer(reinterpret_cast<BYTE*>(context));
}

HMODULE XAudio2::m_xaudio2_dll = nullptr;
typedef decltype(&XAudio2Create) XAudio2Create_t;
void* XAudio2::PXAudio2Create = nullptr;

bool XAudio2::InitLibrary()
{
  if (m_xaudio2_dll)
  {
    return true;
  }

  m_xaudio2_dll = ::LoadLibrary(XAUDIO2_DLL);
  if (!m_xaudio2_dll)
  {
    return false;
  }

  if (!PXAudio2Create)
  {
    PXAudio2Create = (XAudio2Create_t)::GetProcAddress(m_xaudio2_dll, "XAudio2Create");
    if (!PXAudio2Create)
    {
      ::FreeLibrary(m_xaudio2_dll);
      m_xaudio2_dll = nullptr;
      return false;
    }
  }

  return true;
}

XAudio2::XAudio2()
    : m_mastering_voice(nullptr), m_volume(1.0f),
      m_cleanup_com(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
{
}

XAudio2::~XAudio2()
{
  Stop();
  if (m_cleanup_com)
    CoUninitialize();
}

bool XAudio2::Init()
{
  HRESULT hr;

  // callback doesn't seem to run on a specific CPU anyways
  IXAudio2* xaudptr;
  if (FAILED(hr = ((XAudio2Create_t)PXAudio2Create)(&xaudptr, 0, XAUDIO2_DEFAULT_PROCESSOR)))
  {
    PanicAlert("XAudio2 init failed: %#X", hr);
    Stop();
    return false;
  }
  m_xaudio2 = std::unique_ptr<IXAudio2, Releaser>(xaudptr);

  // XAudio2 master voice
  // XAUDIO2_DEFAULT_CHANNELS instead of 2 for expansion?
  uint channels = NUM_CHANNELS;
  if (SConfig::GetInstance().bDPL2Decoder)
    channels = NUM_CHANNELS_SURROUND;

  if (FAILED(hr = m_xaudio2->CreateMasteringVoice(&m_mastering_voice, channels,
                                                  m_mixer->GetSampleRate())))
  {
    PanicAlert("XAudio2 master voice creation failed: %#X", hr);
    Stop();
    return false;
  }

  // Volume
  m_mastering_voice->SetVolume(m_volume);

  m_voice_context = std::unique_ptr<StreamingVoiceContext>(
      new StreamingVoiceContext(m_xaudio2.get(), m_mixer.get(), m_sound_sync_event));

  return true;
}

void XAudio2::SetVolume(int volume)
{
  // linear 1- .01
  m_volume = (float)volume / 100.f;

  if (m_mastering_voice)
    m_mastering_voice->SetVolume(m_volume);
}

bool XAudio2::SetRunning(bool running)
{
  if (!m_voice_context)
    return false;

  if (running)
    m_voice_context->Play();
  else
    m_voice_context->Stop();

  return true;
}

void XAudio2::Stop()
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
    PXAudio2Create = nullptr;
  }
}
