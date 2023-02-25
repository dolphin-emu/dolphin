// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_OPENSL_ES
#include "AudioCommon/OpenSLESStream.h"

#include <cmath>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

// engine interfaces
static SLObjectItf engineObject;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = nullptr;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLVolumeItf bqPlayerVolume;
static Mixer* g_mixer;
#define BUFFER_SIZE 512
#define BUFFER_SIZE_IN_SAMPLES (BUFFER_SIZE / 2)

// Double buffering.
static short buffer[2][BUFFER_SIZE];
static int curBuffer = 0;

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context)
{
  ASSERT(bq == bqPlayerBufferQueue);
  ASSERT(nullptr == context);

  // Render to the fresh buffer
  g_mixer->Mix(reinterpret_cast<short*>(buffer[curBuffer]), BUFFER_SIZE_IN_SAMPLES);
  SLresult result =
      (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], sizeof(buffer[0]));
  curBuffer ^= 1;  // Switch buffer

  // Comment from sample code:
  // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
  // which for this code example would indicate a programming error
  ASSERT_MSG(AUDIO, SL_RESULT_SUCCESS == result, "Couldn't enqueue audio stream.");
}

bool OpenSLESStream::Init()
{
  SLresult result;
  // create engine
  result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
  ASSERT(SL_RESULT_SUCCESS == result);

  SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
  SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                 2,
                                 m_mixer->GetSampleRate() * 1000,
                                 SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                 SL_BYTEORDER_LITTLEENDIAN};

  SLDataSource audioSrc = {&loc_bufq, &format_pcm};

  // configure audio sink
  SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
  SLDataSink audioSnk = {&loc_outmix, nullptr};

  // create audio player
  const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
  const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
  result =
      (*engineEngine)
          ->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
  ASSERT(SL_RESULT_SUCCESS == result);

  result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
  ASSERT(SL_RESULT_SUCCESS == result);
  result =
      (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, nullptr);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
  ASSERT(SL_RESULT_SUCCESS == result);

  // Render and enqueue a first buffer.
  curBuffer ^= 1;
  g_mixer = m_mixer.get();

  result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[0], sizeof(buffer[0]));
  if (SL_RESULT_SUCCESS != result)
    return false;

  return true;
}

OpenSLESStream::~OpenSLESStream()
{
  if (bqPlayerObject != nullptr)
  {
    (*bqPlayerObject)->Destroy(bqPlayerObject);
    bqPlayerObject = nullptr;
    bqPlayerPlay = nullptr;
    bqPlayerBufferQueue = nullptr;
    bqPlayerVolume = nullptr;
  }

  if (outputMixObject != nullptr)
  {
    (*outputMixObject)->Destroy(outputMixObject);
    outputMixObject = nullptr;
  }

  if (engineObject != nullptr)
  {
    (*engineObject)->Destroy(engineObject);
    engineObject = nullptr;
    engineEngine = nullptr;
  }
}

void OpenSLESStream::SetVolume(int volume)
{
  const SLmillibel attenuation =
      volume <= 0 ? SL_MILLIBEL_MIN : static_cast<SLmillibel>(2000 * std::log10(volume / 100.0f));
  (*bqPlayerVolume)->SetVolumeLevel(bqPlayerVolume, attenuation);
}
#endif  // HAVE_OPENSL_ES
