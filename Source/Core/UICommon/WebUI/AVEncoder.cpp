// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/WebUI/AVEncoder.h"

#include <functional>
#include <memory>
#include <span>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Functional.h"
#include "Common/Logging/Log.h"
#include "Common/Matrix.h"
#include "Common/WorkQueueThread.h"

namespace
{
template <auto FreeFunc>
struct InvokerWithAddressOf
{
  auto operator()(auto* ptr) { return FreeFunc(&ptr); }
};

using UniqueAVFrame = std::unique_ptr<AVFrame, InvokerWithAddressOf<av_frame_free>>;
using UniqueAVCodecContext =
    std::unique_ptr<AVCodecContext, InvokerWithAddressOf<avcodec_free_context>>;
using UniqueAVPacket = std::unique_ptr<AVPacket, InvokerWithAddressOf<av_packet_free>>;
using UniqueSwsContext = std::unique_ptr<SwsContext, Common::InvokerOf<sws_freeContext>>;

template <typename Base>
class AVCodecEncoder : public Base
{
protected:
  explicit AVCodecEncoder(std::string name) : m_name{std::move(name)} {}

  // A nullptr drains and closes the context.
  void PushAVFrame(AVFrame* av_frame) { m_worker.Push(av_frame); }

  // Wait for the worker to be done with av_frame that were provided via PushAVFrame.
  void WaitForCompletion() { m_worker.WaitForCompletion(); }

  // Invoked from the worker thread on start or after a flush.
  virtual UniqueAVCodecContext PrepareAVCodec() = 0;

  // Invoked from the worker after a frame is sent to the context.
  virtual void AfterSendFrame(AVFrame* av_frame) = 0;

private:
  void EncodeAndReceive(AVFrame* av_frame)
  {
    if (av_frame != nullptr)
    {
      if (!m_codec_context)
      {
        m_codec_context = PrepareAVCodec();
        if (!m_codec_context)
        {
          ERROR_LOG_FMT(COMMON, "{}: Could not find encoder", m_name);
          return;
        }

        INFO_LOG_FMT(COMMON, "{}: Found codec: {}", m_name, m_codec_context->codec->name);
      }

      SendFrameAndReceivePackets(av_frame);
      AfterSendFrame(av_frame);
    }
    else if (m_codec_context != nullptr)
    {
      INFO_LOG_FMT(COMMON, "{}: Draining context", m_name);
      SendFrameAndReceivePackets(nullptr);
      m_codec_context.reset();
    }
  }

  void SendFrameAndReceivePackets(AVFrame* av_frame)
  {
    DEBUG_ASSERT(m_codec_context);

    if (avcodec_send_frame(m_codec_context.get(), av_frame) < 0)
      ERROR_LOG_FMT(COMMON, "{}: avcodec_send_frame", m_name);

    while (avcodec_receive_packet(m_codec_context.get(), m_received_packet.get()) == 0)
    {
      this->m_packet_callback({m_received_packet->data, std::size_t(m_received_packet->size)},
                              m_received_packet->pts);

      av_packet_unref(m_received_packet.get());
    }
  }

  UniqueAVPacket m_received_packet{av_packet_alloc()};
  UniqueAVCodecContext m_codec_context;

  const std::string m_name;
  Common::WorkQueueThreadSP<AVFrame*> m_worker{
      m_name, std::bind_front(&AVCodecEncoder::EncodeAndReceive, this)};
};

class OpusEncoder final : public AVCodecEncoder<WebUI::AudioEncoder>
{
public:
  OpusEncoder() : AVCodecEncoder<WebUI::AudioEncoder>{"OpusEncoder"} {}

  void
  TakeAudioSamples(std::size_t frame_count,
                   Common::MoveOnlyFunction<void(std::span<float>)> fill_buffer_callback) override
  {
    while (frame_count != 0)
    {
      const auto frames_to_take =
          std::min<std::size_t>(frame_count, AV_FRAME_BUFFER_SIZE - m_buffered_frames);

      if (m_buffered_frames == 0)
        PrepareAVFrame();

      const std::size_t samples_per_frame = m_av_frame->ch_layout.nb_channels;

      fill_buffer_callback(std::span{reinterpret_cast<float*>(m_av_frame->data[0]) +
                                         (m_buffered_frames * samples_per_frame),
                                     frames_to_take * samples_per_frame});

      m_buffered_frames += int(frames_to_take);

      // AVCodecContext requires sending full buffers until the final flush.
      if (m_buffered_frames < AV_FRAME_BUFFER_SIZE)
        break;

      PushBufferedFrames();

      frame_count -= frames_to_take;
    }
  }

  void Flush() override
  {
    PushBufferedFrames();
    PushAVFrame(nullptr);
  }

protected:
  UniqueAVCodecContext PrepareAVCodec() override
  {
    const AVCodec* codec = avcodec_find_encoder_by_name("libopus");
    if (!codec)
      codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec)
      return nullptr;

    UniqueAVCodecContext context{avcodec_alloc_context3(codec)};
    context->sample_rate = WebUI::AUDIO_SAMPLE_RATE;
    context->sample_fmt = AVSampleFormat(m_av_frame->format);
    context->bit_rate = BIT_RATE;
    context->time_base = AVRational{1, WebUI::AUDIO_SAMPLE_RATE};
    av_channel_layout_copy(&context->ch_layout, &m_av_frame->ch_layout);

    av_opt_set(context->priv_data, "application", "lowdelay", 0);
    av_opt_set(context->priv_data, "frame_duration", "20", 0);

    if (avcodec_open2(context.get(), codec, nullptr) < 0)
    {
      ERROR_LOG_FMT(COMMON, "OpusEncoder: avcodec_open2");
      return nullptr;
    }

    return context;
  }

  void AfterSendFrame(AVFrame* av_frame) override
  {
    // Advance PTS by number of audio frames.
    av_frame->pts += av_frame->nb_samples;
  }

private:
  static constexpr int BIT_RATE = 96'000;
  static constexpr int CHANNEL_COUNT = 2;
  static constexpr int AV_FRAME_BUFFER_MS = 20;
  static constexpr int AV_FRAME_BUFFER_SIZE = WebUI::AUDIO_SAMPLE_RATE * AV_FRAME_BUFFER_MS / 1000;

  void PushBufferedFrames()
  {
    if (m_buffered_frames == 0)
      return;

    m_av_frame->nb_samples = std::exchange(m_buffered_frames, 0);
    PushAVFrame(m_av_frame.get());
  }

  void PrepareAVFrame()
  {
    if (m_av_frame != nullptr)
    {
      WaitForCompletion();

      if (av_frame_make_writable(m_av_frame.get()) < 0)
        ERROR_LOG_FMT(COMMON, "OpusEncoder: av_frame_make_writable");
      return;
    }

    m_av_frame.reset(av_frame_alloc());
    m_av_frame->format = AV_SAMPLE_FMT_FLT;
    m_av_frame->sample_rate = WebUI::AUDIO_SAMPLE_RATE;
    m_av_frame->nb_samples = AV_FRAME_BUFFER_SIZE;
    av_channel_layout_default(&m_av_frame->ch_layout, CHANNEL_COUNT);

    if (av_frame_get_buffer(m_av_frame.get(), 0) < 0)
      ERROR_LOG_FMT(COMMON, "OpusEncoder: av_frame_get_buffer");

    m_av_frame->pts = 0;
  }

  int m_buffered_frames = 0;

  UniqueAVFrame m_av_frame;
};

class H264Encoder final : public AVCodecEncoder<WebUI::VideoEncoder>
{
public:
  H264Encoder() : AVCodecEncoder<WebUI::VideoEncoder>{"H264Encoder"} {}

  void PushVideoFrame(FrameDetails frame) override
  {
    const FrameSize frame_size{frame.mdspan.extent(1), frame.mdspan.extent(0)};

    PrepareAVFrame(frame_size);

    // Convert from RGB to YUV and scale if necessary.
    m_sws_context.reset(
        sws_getCachedContext(m_sws_context.release(), frame_size.x, frame_size.y, AV_PIX_FMT_RGB0,
                             m_av_frame->width, m_av_frame->height, PIXEL_FORMAT,
                             SWS_POINT | SWS_ACCURATE_RND | SWS_BITEXACT | SWS_FULL_CHR_H_INP,
                             nullptr, nullptr, nullptr));

    const u8* const src_data = frame.mdspan.data_handle();
    const int src_stride = frame.mdspan.stride(0) * 4;
    sws_scale(m_sws_context.get(), &src_data, &src_stride, 0, frame_size.y, m_av_frame->data,
              m_av_frame->linesize);

    PushAVFrame(m_av_frame.get());
  }

  void Flush() override { PushAVFrame(nullptr); }

protected:
  UniqueAVCodecContext PrepareAVCodec() override
  {
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
      codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
      return nullptr;

    // FYI: We currently never change this. It won't affect playback timing.
    // It should only affect things like keyframe frequency.
    constexpr int video_framerate = 60;

    UniqueAVCodecContext context{avcodec_alloc_context3(codec)};
    context->width = m_av_frame->width;
    context->height = m_av_frame->height;
    context->time_base = AVRational{1, video_framerate};
    context->framerate = AVRational{video_framerate, 1};
    context->pix_fmt = PIXEL_FORMAT;
    context->gop_size = video_framerate / 2;  // Half-second.
    context->max_b_frames = 0;
    context->flags |= AV_CODEC_FLAG_LOW_DELAY;

    av_opt_set(context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(context->priv_data, "repeat-headers", "1", 0);
    av_opt_set(context->priv_data, "keyint", "60", 0);
    av_opt_set(context->priv_data, "min-keyint", "60", 0);
    av_opt_set(context->priv_data, "scenecut", "0", 0);
    av_opt_set(context->priv_data, "crf", "20", 0);

    if (avcodec_open2(context.get(), codec, nullptr) < 0)
    {
      ERROR_LOG_FMT(COMMON, "H264Encoder: avcodec_open2");
      return nullptr;
    }

    return context;
  }

  void AfterSendFrame(AVFrame* av_frame) override
  {
    // Advance PTS by number of video frames.
    ++av_frame->pts;
  }

private:
  // YUV444 is not widely supported in browsers, though it does work on Linux.
  static constexpr bool USE_YUV444 = false;

  static constexpr AVPixelFormat PIXEL_FORMAT =
      USE_YUV444 ? AV_PIX_FMT_YUV444P : AV_PIX_FMT_YUV420P;

  // GBA pixel art tends to look terrible without doing this.
  static constexpr bool PIXEL_ACCURATE_CHROMA = true;

  using FrameSize = Common::TVec2<int>;

  void PrepareAVFrame(FrameSize resolution)
  {
    auto video_size = resolution;

    if (PIXEL_ACCURATE_CHROMA && !USE_YUV444)
    {
      // Compensate for chroma subsampling by doubling the resolution.
      video_size *= 2;
    }

    // Maintain the PTS if we have to re-create the AVFrame.
    s64 stable_pts = 0;

    if (m_av_frame != nullptr)
    {
      WaitForCompletion();

      const bool existing_av_frame_matches =
          m_av_frame->width == video_size.x && m_av_frame->height == video_size.y;
      if (existing_av_frame_matches)
      {
        if (av_frame_make_writable(m_av_frame.get()) < 0)
          ERROR_LOG_FMT(COMMON, "H264Encoder: av_frame_make_writable");
        return;
      }

      // Drain the context and re-create the AVFrame with the new size.
      Flush();

      stable_pts = m_av_frame->pts;
      m_av_frame.reset();
    }

    m_av_frame.reset(av_frame_alloc());
    m_av_frame->format = PIXEL_FORMAT;
    m_av_frame->width = video_size.x;
    m_av_frame->height = video_size.y;

    if (av_frame_get_buffer(m_av_frame.get(), 0) < 0)
      ERROR_LOG_FMT(COMMON, "H264Encoder: av_frame_get_buffer");

    m_av_frame->pts = stable_pts;
  }

  UniqueAVFrame m_av_frame;
  UniqueSwsContext m_sws_context;
};

}  // namespace

namespace WebUI
{

AVPacketProducer::~AVPacketProducer() = default;

void AVPacketProducer::SetPacketCallback(ReceivePacketCallbackType&& callback)
{
  m_packet_callback = std::move(callback);
}

std::unique_ptr<AudioEncoder> CreateOpusEncoder()
{
  return std::make_unique<OpusEncoder>();
}

std::unique_ptr<VideoEncoder> CreateH264Encoder()
{
  return std::make_unique<H264Encoder>();
}

}  // namespace WebUI
