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

void OpenSLESStream::BQPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context)
{
  reinterpret_cast<OpenSLESStream*>(context)->PushSamples(bq);
}

void OpenSLESStream::PushSamples(SLAndroidSimpleBufferQueueItf bq)
{
  ASSERT(bq == m_bq_player_buffer_queue);

  // Render to the fresh buffer
  m_mixer->Mix(reinterpret_cast<short*>(m_buffer[m_current_buffer]), BUFFER_SIZE_IN_SAMPLES);
  SLresult result = (*bq)->Enqueue(bq, m_buffer[m_current_buffer], sizeof(m_buffer[0]));
  m_current_buffer ^= 1;  // Switch buffer

  // Comment from sample code:
  // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
  // which for this code example would indicate a programming error
  ASSERT_MSG(AUDIO, SL_RESULT_SUCCESS == result, "Couldn't enqueue audio stream.");
}

bool OpenSLESStream::Init()
{
  SLresult result;
  // create engine
  result = slCreateEngine(&m_engine_object, 0, nullptr, 0, nullptr, nullptr);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_engine_object)->Realize(m_engine_object, SL_BOOLEAN_FALSE);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_engine_object)->GetInterface(m_engine_object, SL_IID_ENGINE, &m_engine_engine);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_engine_engine)->CreateOutputMix(m_engine_engine, &m_output_mix_object, 0, 0, 0);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_output_mix_object)->Realize(m_output_mix_object, SL_BOOLEAN_FALSE);
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
  SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, m_output_mix_object};
  SLDataSink audioSnk = {&loc_outmix, nullptr};

  // create audio player
  const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
  const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
  result = (*m_engine_engine)
               ->CreateAudioPlayer(m_engine_engine, &m_bq_player_object, &audioSrc, &audioSnk, 2,
                                   ids, req);
  ASSERT(SL_RESULT_SUCCESS == result);

  result = (*m_bq_player_object)->Realize(m_bq_player_object, SL_BOOLEAN_FALSE);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_bq_player_object)->GetInterface(m_bq_player_object, SL_IID_PLAY, &m_bq_player_play);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_bq_player_object)
               ->GetInterface(m_bq_player_object, SL_IID_BUFFERQUEUE, &m_bq_player_buffer_queue);
  ASSERT(SL_RESULT_SUCCESS == result);
  result =
      (*m_bq_player_object)->GetInterface(m_bq_player_object, SL_IID_VOLUME, &m_bq_player_volume);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_bq_player_buffer_queue)
               ->RegisterCallback(m_bq_player_buffer_queue, BQPlayerCallback, this);
  ASSERT(SL_RESULT_SUCCESS == result);
  result = (*m_bq_player_play)->SetPlayState(m_bq_player_play, SL_PLAYSTATE_PLAYING);
  ASSERT(SL_RESULT_SUCCESS == result);

  // Render and enqueue a first buffer.
  m_current_buffer ^= 1;

  result = (*m_bq_player_buffer_queue)
               ->Enqueue(m_bq_player_buffer_queue, m_buffer[0], sizeof(m_buffer[0]));
  if (SL_RESULT_SUCCESS != result)
    return false;

  return true;
}

OpenSLESStream::~OpenSLESStream()
{
  if (m_bq_player_object != nullptr)
  {
    (*m_bq_player_object)->Destroy(m_bq_player_object);
    m_bq_player_object = nullptr;
    m_bq_player_play = nullptr;
    m_bq_player_buffer_queue = nullptr;
    m_bq_player_volume = nullptr;
  }

  if (m_output_mix_object != nullptr)
  {
    (*m_output_mix_object)->Destroy(m_output_mix_object);
    m_output_mix_object = nullptr;
  }

  if (m_engine_object != nullptr)
  {
    (*m_engine_object)->Destroy(m_engine_object);
    m_engine_object = nullptr;
    m_engine_engine = nullptr;
  }
}

bool OpenSLESStream::SetRunning(bool running)
{
  SLuint32 new_state = running ? SL_PLAYSTATE_PLAYING : SL_PLAYSTATE_PAUSED;
  return (*m_bq_player_play)->SetPlayState(m_bq_player_play, new_state) == SL_RESULT_SUCCESS;
}

void OpenSLESStream::SetVolume(int volume)
{
  const SLmillibel attenuation =
      volume <= 0 ? SL_MILLIBEL_MIN : static_cast<SLmillibel>(2000 * std::log10(volume / 100.0f));
  (*m_bq_player_volume)->SetVolumeLevel(m_bq_player_volume, attenuation);
}
#endif  // HAVE_OPENSL_ES
