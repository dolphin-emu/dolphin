// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/WebUI/AVStream.h"

#include <mutex>

#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"

namespace
{

// A container of `weak_ptr` that many threads can append items to.
template <typename T>
class ManyProducerWeakVector
{
public:
  // May be called concurrently.
  void Append(std::weak_ptr<T> track)
  {
    std::lock_guard lk{m_producer_mutex};
    m_new_items.Push(std::move(track));
  }

  // Only safe from a single thread.
  void ForEach(std::invocable<T&> auto&& callback)
  {
    for (std::weak_ptr<T> item; m_new_items.Pop(item);)
      m_items.emplace_back(std::move(item));

    std::erase_if(m_items, [&](auto&& ptr) {
      if (const auto sptr = ptr.lock())
      {
        callback(*sptr);
        return false;
      }
      return true;
    });
  }

private:
  std::mutex m_producer_mutex;
  Common::SPSCQueue<std::weak_ptr<T>> m_new_items;
  std::vector<std::weak_ptr<T>> m_items;
};

}  // namespace

namespace WebUI
{

struct AVStream::Impl
{
  void InvokeAudioHandlers(std::span<const u8> data, s64 pts)
  {
    m_audio_handlers.ForEach([&](PacketHandler& receiver) { receiver(data, pts); });
  }

  void InvokeVideoHandlers(std::span<const u8> data, s64 pts)
  {
    m_video_handlers.ForEach([&](PacketHandler& receiver) { receiver(data, pts); });
  }

  ManyProducerWeakVector<AVStream::PacketHandler> m_audio_handlers;
  ManyProducerWeakVector<AVStream::PacketHandler> m_video_handlers;
};

AVStream::AVStream() : m_impl{std::make_unique<Impl>()}
{
  INFO_LOG_FMT(COMMON, "AVStream: Constructed");

  m_audio_encoder = CreateOpusEncoder();
  m_audio_encoder->SetPacketCallback(std::bind_front(&Impl::InvokeAudioHandlers, m_impl.get()));

  m_video_encoder = CreateH264Encoder();
  m_video_encoder->SetPacketCallback(std::bind_front(&Impl::InvokeVideoHandlers, m_impl.get()));
}

AVStream::~AVStream()
{
  INFO_LOG_FMT(COMMON, "AVStream: Stopped");
}

void AVStream::TakeAudioSamples(
    std::size_t frame_count, Common::MoveOnlyFunction<void(std::span<float>)> fill_buffer_callback)
{
  const auto start_time = Clock::now();

  m_audio_encoder->TakeAudioSamples(frame_count, std::move(fill_buffer_callback));

  const auto stall_time = Clock::now() - start_time;
  // FYI: Every once in a while these stall logs do trigger.
  //  If it becomes a problem, AVEncoder should probably buffer more than one AVFrame.
  if (stall_time > std::chrono::milliseconds{1})
    WARN_LOG_FMT(COMMON, "AVStream: Audio stalled us for {:.2f} ms", DT_ms{stall_time}.count());
}

void AVStream::PushVideoFrame(VideoEncoder::FrameDetails frame)
{
  const auto start_time = Clock::now();

  m_video_encoder->PushVideoFrame(frame);

  const auto stall_time = Clock::now() - start_time;
  if (stall_time > std::chrono::milliseconds{1})
    WARN_LOG_FMT(COMMON, "AVStream: Video stalled us for {:.2f} ms", DT_ms{stall_time}.count());
}

void AVStream::FlushVideo()
{
  m_video_encoder->Flush();
}

void AVStream::AddAudioPacketHandler(std::weak_ptr<PacketHandler> receiver)
{
  m_impl->m_audio_handlers.Append(std::move(receiver));
}

void AVStream::AddVideoPacketHandler(std::weak_ptr<PacketHandler> receiver)
{
  m_impl->m_video_handlers.Append(std::move(receiver));
}

}  // namespace WebUI
