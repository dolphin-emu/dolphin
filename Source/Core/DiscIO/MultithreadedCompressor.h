// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "Common/Assert.h"
#include "Common/Event.h"
#include "Common/Result.h"

namespace DiscIO
{
enum class ConversionResultCode
{
  Success,
  Canceled,
  ReadFailed,
  WriteFailed,
  InternalError,
};

template <typename T>
using ConversionResult = Common::Result<ConversionResultCode, T>;

// This class starts a number of compression threads and one output thread.
// The set_up_compress_thread_state function is called at the start of each compression thread.
// When CompressAndWrite is called, the compress function will be called on one of the
// compression threads, and then the output function will be called on the output thread.
// The output thread handles data in the order that data was submitted using CompressAndWrite,
// but the compression threads are not guaranteed to handle data in a predictable order.
// Remember to check GetStatus regularly and cancel if it doesn't return Success,
// and call Shutdown when you want to ensure that everything finishes.
template <typename CompressThreadState, typename CompressParameters, typename OutputParameters>
class MultithreadedCompressor
{
public:
  MultithreadedCompressor(
      std::function<ConversionResultCode(CompressThreadState*)> set_up_compress_thread_state,
      std::function<ConversionResult<OutputParameters>(CompressThreadState*, CompressParameters)>
          compress,
      std::function<ConversionResultCode(OutputParameters)> output)
      : m_set_up_compress_thread_state(std::move(set_up_compress_thread_state)),
        m_compress(std::move(compress)), m_output(std::move(output)),
        m_threads(std::max<unsigned int>(1, std::thread::hardware_concurrency()))
  {
    m_compress_threads = std::make_unique<CompressThread[]>(m_threads);

    for (size_t i = 0; i < m_threads; ++i)
    {
      m_compress_threads[i].thread =
          std::thread(std::mem_fn(&MultithreadedCompressor::CompressThreadFunction), this,
                      &m_compress_threads[i]);
    }

    m_output_thread =
        std::thread(std::mem_fn(&MultithreadedCompressor::OutputThreadFunction), this);
  }

  ~MultithreadedCompressor()
  {
    if (!m_shutting_down.load())
      Shutdown();
  }

  void CompressAndWrite(CompressParameters parameters)
  {
    if (GetStatus() != ConversionResultCode::Success)
      return;

    CompressThread& compress_thread = m_compress_threads[m_current_index];

    compress_thread.compress_ready_event.Wait();
    compress_thread.compress_parameters = std::move(parameters);
    compress_thread.compress_event.Set();

    ++m_current_index;
    if (m_current_index >= m_threads)
      m_current_index -= m_threads;
  }

  void SetError(ConversionResultCode result)
  {
    ASSERT(result != ConversionResultCode::Success);

    // If we already have an error, don't overwrite it
    ConversionResultCode expected = ConversionResultCode::Success;
    m_result.compare_exchange_strong(expected, result);
  }

  ConversionResultCode GetStatus() const { return m_result.load(); }

  void Shutdown()
  {
    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].compress_ready_event.Wait();
    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].compress_done_event.Wait();
    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].output_ready_event.Wait();

    m_shutting_down.store(true);

    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].compress_event.Set();
    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].output_event.Set();

    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].thread.join();

    m_output_thread.join();
  }

private:
  struct CompressThread
  {
    std::thread thread;

    Common::Event compress_ready_event;
    Common::Event compress_event;
    Common::Event compress_done_event;
    Common::Event output_ready_event;
    Common::Event output_event;

    CompressParameters compress_parameters;
    OutputParameters output_parameters;
  };

  void CompressThreadFunction(CompressThread* state)
  {
    CompressThreadState compress_thread_state;

    ConversionResultCode setup_result = m_set_up_compress_thread_state(&compress_thread_state);
    if (setup_result != ConversionResultCode::Success)
      SetError(setup_result);

    state->compress_ready_event.Set();
    state->compress_done_event.Set();

    while (true)
    {
      state->compress_event.Wait();

      if (m_shutting_down.load())
        return;

      CompressParameters parameters = std::move(state->compress_parameters);

      state->compress_done_event.Reset();
      state->compress_ready_event.Set();

      ConversionResult<OutputParameters> result =
          m_compress(&compress_thread_state, std::move(parameters));

      if (result)
      {
        state->output_ready_event.Wait();
        state->output_parameters = std::move(*result);
        state->output_event.Set();
      }
      else
      {
        SetError(result.Error());
      }

      state->compress_done_event.Set();
    }
  }

  void OutputThreadFunction()
  {
    for (size_t i = 0; i < m_threads; ++i)
      m_compress_threads[i].output_ready_event.Set();

    size_t index = 0;

    while (true)
    {
      CompressThread& compress_thread = m_compress_threads[index];

      compress_thread.output_event.Wait();

      if (m_shutting_down.load())
        return;

      OutputParameters parameters = std::move(compress_thread.output_parameters);

      compress_thread.output_ready_event.Set();

      const ConversionResultCode result = m_output(std::move(parameters));

      if (result != ConversionResultCode::Success)
        SetError(result);

      ++index;
      if (index >= m_threads)
        index -= m_threads;
    }
  }

  std::function<ConversionResultCode(CompressThreadState*)> m_set_up_compress_thread_state;
  std::function<ConversionResult<OutputParameters>(CompressThreadState*, CompressParameters)>
      m_compress;
  std::function<ConversionResultCode(OutputParameters)> m_output;

  // We can't use std::vector for this, because Common::Event is not movable
  std::unique_ptr<CompressThread[]> m_compress_threads;
  std::thread m_output_thread;

  const size_t m_threads;
  size_t m_current_index = 0;

  std::atomic<ConversionResultCode> m_result = ConversionResultCode::Success;
  std::atomic<bool> m_shutting_down = false;
};

}  // namespace DiscIO
