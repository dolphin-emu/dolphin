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

// Matches Dolphin's Mixer.
constexpr int AUDIO_SAMPLE_RATE = 48'000;

template <typename Base>
class AVCodecEncoder : public Base
{
protected:
  void ReceivePackets()
  {
    while (avcodec_receive_packet(m_codec_context.get(), m_received_packet.get()) == 0)
    {
      this->m_packet_callback({m_received_packet->data, std::size_t(m_received_packet->size)},
                              m_received_packet->pts);

      av_packet_unref(m_received_packet.get());
    }
  }

  UniqueAVCodecContext m_codec_context;

private:
  UniqueAVPacket m_received_packet{av_packet_alloc()};
};

class OpusEncoder final : public AVCodecEncoder<WebUI::AudioEncoder>
{
public:
  void TakeAudioSamples(std::size_t frame_count,
                        Common::MoveOnlyFunction<void(std::span<u8>)> fill_buffer_callback) override
  {
    while (frame_count != 0)
    {
      const auto frames_to_take =
          std::min<std::size_t>(frame_count, AV_FRAME_BUFFER_SIZE - m_buffered_frames);

      if (m_buffered_frames == 0)
        PrepareAVFrame();

      const std::size_t bytes_per_frame = m_av_frame->ch_layout.nb_channels * sizeof(float);

      fill_buffer_callback(std::span{m_av_frame->data[0] + (m_buffered_frames * bytes_per_frame),
                                     frames_to_take * bytes_per_frame});

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
    m_worker.Push(nullptr);
  }

private:
  static constexpr int BIT_RATE = 96'000;
  static constexpr int CHANNEL_COUNT = 2;
  static constexpr int AV_FRAME_BUFFER_MS = 20;
  static constexpr int AV_FRAME_BUFFER_SIZE = AUDIO_SAMPLE_RATE * AV_FRAME_BUFFER_MS / 1000;

  void PushBufferedFrames()
  {
    if (m_buffered_frames == 0)
      return;

    m_av_frame->nb_samples = std::exchange(m_buffered_frames, 0);
    m_worker.Push(m_av_frame.get());
  }

  void PrepareAVFrame()
  {
    if (m_av_frame != nullptr)
    {
      m_worker.WaitForCompletion();

      if (av_frame_make_writable(m_av_frame.get()) < 0)
        ERROR_LOG_FMT(COMMON, "TakeAudioSamples: av_frame_make_writable");
      return;
    }

    m_av_frame.reset(av_frame_alloc());
    m_av_frame->format = AV_SAMPLE_FMT_FLT;
    m_av_frame->sample_rate = AUDIO_SAMPLE_RATE;
    m_av_frame->nb_samples = AV_FRAME_BUFFER_SIZE;
    av_channel_layout_default(&m_av_frame->ch_layout, CHANNEL_COUNT);

    if (av_frame_get_buffer(m_av_frame.get(), 0) < 0)
      ERROR_LOG_FMT(COMMON, "OpusEncoder: av_frame_get_buffer");

    m_av_frame->pts = 0;
  }

  void PrepareAVCodec()
  {
    if (m_codec_context != nullptr)
      return;

    const AVCodec* codec = avcodec_find_encoder_by_name("libopus");
    if (!codec)
      codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec)
      ERROR_LOG_FMT(COMMON, "OpusEncoder: avcodec_find_encoder");

    INFO_LOG_FMT(COMMON, "OpusEncoder: Found codec: {}", codec->name);

    m_codec_context.reset(avcodec_alloc_context3(codec));
    m_codec_context->sample_rate = AUDIO_SAMPLE_RATE;
    m_codec_context->sample_fmt = AVSampleFormat(m_av_frame->format);
    m_codec_context->bit_rate = BIT_RATE;
    m_codec_context->time_base = AVRational{1, AUDIO_SAMPLE_RATE};
    av_channel_layout_copy(&m_codec_context->ch_layout, &m_av_frame->ch_layout);

    av_opt_set(m_codec_context->priv_data, "application", "lowdelay", 0);
    av_opt_set(m_codec_context->priv_data, "frame_duration", "20", 0);

    if (avcodec_open2(m_codec_context.get(), codec, nullptr) < 0)
      ERROR_LOG_FMT(COMMON, "OpusEncoder: avcodec_open2");
  }

  void EncodeAndReceive(AVFrame* av_frame)
  {
    if (av_frame != nullptr)
    {
      PrepareAVCodec();

      if (avcodec_send_frame(m_codec_context.get(), av_frame) < 0)
        ERROR_LOG_FMT(COMMON, "OpusEncoder: avcodec_send_frame");
      ReceivePackets();

      av_frame->pts += av_frame->nb_samples;
    }
    else if (m_codec_context != nullptr)
    {
      INFO_LOG_FMT(COMMON, "OpusEncoder: Draining context");
      avcodec_send_frame(m_codec_context.get(), nullptr);
      ReceivePackets();
      m_codec_context.reset();
    }
  }

  int m_buffered_frames = 0;

  UniqueAVFrame m_av_frame;

  Common::WorkQueueThreadSP<AVFrame*> m_worker{
      "OpusEncoder", std::bind_front(&OpusEncoder::EncodeAndReceive, this)};
};

class H264Encoder final : public AVCodecEncoder<WebUI::VideoEncoder>
{
public:
  void PushVideoFrame(FrameDetails frame) override
  {
    PrepareAVFrame(frame.size);

    // Convert from RGB to YUV and scale if necessary.
    m_sws_context.reset(
        sws_getCachedContext(m_sws_context.release(), frame.size.x, frame.size.y, AV_PIX_FMT_RGB0,
                             m_av_frame->width, m_av_frame->height, PIXEL_FORMAT,
                             SWS_POINT | SWS_ACCURATE_RND | SWS_BITEXACT | SWS_FULL_CHR_H_INP,
                             nullptr, nullptr, nullptr));

    const int src_stride = frame.size.x * 4;
    sws_scale(m_sws_context.get(), &frame.data, &src_stride, 0, frame.size.y, m_av_frame->data,
              m_av_frame->linesize);

    m_worker.Push(m_av_frame.get());
  }

  void Flush() override { m_worker.Push(nullptr); }

private:
  // YUV444 is not widely supported in browsers, though it does work on Linux.
  static constexpr bool USE_YUV444 = false;

  static constexpr AVPixelFormat PIXEL_FORMAT =
      USE_YUV444 ? AV_PIX_FMT_YUV444P : AV_PIX_FMT_YUV420P;

  // GBA pixel art tends to look terrible without doing this.
  static constexpr bool PIXEL_ACCURATE_CHROMA = true;

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
      m_worker.WaitForCompletion();

      const bool existing_av_frame_matches =
          m_av_frame->width == video_size.x && m_av_frame->height == video_size.y;
      if (existing_av_frame_matches)
      {
        if (av_frame_make_writable(m_av_frame.get()) < 0)
          ERROR_LOG_FMT(COMMON, "PushVideoFrame: av_frame_make_writable");
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
      ERROR_LOG_FMT(COMMON, "H264VideoEncoder: av_frame_get_buffer");

    m_av_frame->pts = stable_pts;
  }

  void PrepareAVCodec()
  {
    if (m_codec_context != nullptr)
      return;

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
      codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
      ERROR_LOG_FMT(COMMON, "H264VideoEncoder: avcodec_find_encoder");

    INFO_LOG_FMT(COMMON, "H264Encoder: Found codec: {}", codec->name);

    // FYI: We currently never change this. It won't affect playback timing.
    // It should only affect things like keyframe frequency.
    constexpr int video_framerate = 60;

    m_codec_context.reset(avcodec_alloc_context3(codec));
    m_codec_context->width = m_av_frame->width;
    m_codec_context->height = m_av_frame->height;
    m_codec_context->time_base = AVRational{1, video_framerate};
    m_codec_context->framerate = AVRational{video_framerate, 1};
    m_codec_context->pix_fmt = PIXEL_FORMAT;
    m_codec_context->gop_size = video_framerate / 2;  // Half-second.
    m_codec_context->max_b_frames = 0;
    m_codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY;

    av_opt_set(m_codec_context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_codec_context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_codec_context->priv_data, "repeat-headers", "1", 0);
    av_opt_set(m_codec_context->priv_data, "keyint", "60", 0);
    av_opt_set(m_codec_context->priv_data, "min-keyint", "60", 0);
    av_opt_set(m_codec_context->priv_data, "scenecut", "0", 0);
    av_opt_set(m_codec_context->priv_data, "crf", "20", 0);

    if (avcodec_open2(m_codec_context.get(), codec, nullptr) < 0)
      ERROR_LOG_FMT(COMMON, "H264VideoEncoder: avcodec_open2");
  }

  void EncodeAndReceive(AVFrame* av_frame)
  {
    if (av_frame != nullptr)
    {
      PrepareAVCodec();

      if (avcodec_send_frame(m_codec_context.get(), av_frame) < 0)
        ERROR_LOG_FMT(COMMON, "H264Encoder: avcodec_send_frame");
      ReceivePackets();

      ++av_frame->pts;
    }
    else if (m_codec_context != nullptr)
    {
      INFO_LOG_FMT(COMMON, "H264Encoder: Draining context");
      avcodec_send_frame(m_codec_context.get(), nullptr);
      ReceivePackets();
      m_codec_context.reset();
    }
  }

  UniqueAVFrame m_av_frame;
  UniqueSwsContext m_sws_context;

  Common::WorkQueueThreadSP<AVFrame*> m_worker{
      "H264Encoder", std::bind_front(&H264Encoder::EncodeAndReceive, this)};
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
