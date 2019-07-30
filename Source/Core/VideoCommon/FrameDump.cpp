// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif

#include <sstream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
}

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"  //for TargetRefreshRate
#include "Core/Movie.h"

#include "VideoCommon/FrameDump.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
#define AV_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

static AVFormatContext* s_format_context = nullptr;
static AVStream* s_stream = nullptr;
static AVCodecContext* s_codec_context = nullptr;
static AVFrame* s_src_frame = nullptr;
static AVFrame* s_scaled_frame = nullptr;
static AVPixelFormat s_pix_fmt = AV_PIX_FMT_BGR24;
static SwsContext* s_sws_context = nullptr;
static int s_width;
static int s_height;
static u64 s_last_frame;
static bool s_last_frame_is_valid = false;
static bool s_start_dumping = false;
static u64 s_last_pts;
static int s_file_index = 0;
static int s_savestate_index = 0;
static int s_last_savestate_index = 0;

static void InitAVCodec()
{
  static bool first_run = true;
  if (first_run)
  {
#if LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();
    first_run = false;
  }
}

static bool AVStreamCopyContext(AVStream* stream, AVCodecContext* codec_context)
{
#if (LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 33, 100)) ||  \
    (LIBAVCODEC_VERSION_MICRO < 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 5, 0))

  stream->time_base = codec_context->time_base;
  return avcodec_parameters_from_context(stream->codecpar, codec_context) >= 0;
#else
  return avcodec_copy_context(stream->codec, codec_context) >= 0;
#endif
}

bool FrameDump::Start(int w, int h)
{
  s_pix_fmt = AV_PIX_FMT_RGBA;

  s_width = w;
  s_height = h;

  s_last_frame_is_valid = false;
  s_last_pts = 0;

  InitAVCodec();
  bool success = CreateVideoFile();
  if (!success)
  {
    CloseVideoFile();
    OSD::AddMessage("FrameDump Start failed");
  }
  return success;
}

static std::string GetDumpPath(const std::string& format)
{
  if (!g_Config.sDumpPath.empty())
    return g_Config.sDumpPath;

  const std::string dump_path = File::GetUserPath(D_DUMPFRAMES_IDX) + "framedump" +
                                std::to_string(s_file_index) + "." + format;

  // Ask to delete file
  if (File::Exists(dump_path))
  {
    if (SConfig::GetInstance().m_DumpFramesSilent ||
        AskYesNoT("Delete the existing file '%s'?", dump_path.c_str()))
    {
      File::Delete(dump_path);
    }
    else
    {
      // Stop and cancel dumping the video
      return "";
    }
  }

  return dump_path;
}

bool FrameDump::CreateVideoFile()
{
  const std::string& format = g_Config.sDumpFormat;

  const std::string dump_path = GetDumpPath(format);

  if (dump_path.empty())
    return false;

  File::CreateFullPath(dump_path);

  AVOutputFormat* output_format = av_guess_format(format.c_str(), dump_path.c_str(), nullptr);
  if (!output_format)
  {
    ERROR_LOG(VIDEO, "Invalid format %s", format.c_str());
    return false;
  }

  if (avformat_alloc_output_context2(&s_format_context, output_format, nullptr, dump_path.c_str()) <
      0)
  {
    ERROR_LOG(VIDEO, "Could not allocate output context");
    return false;
  }

  const std::string& codec_name = g_Config.bUseFFV1 ? "ffv1" : g_Config.sDumpCodec;

  AVCodecID codec_id = output_format->video_codec;

  if (!codec_name.empty())
  {
    const AVCodecDescriptor* codec_desc = avcodec_descriptor_get_by_name(codec_name.c_str());
    if (codec_desc)
      codec_id = codec_desc->id;
    else
      WARN_LOG(VIDEO, "Invalid codec %s", codec_name.c_str());
  }

  const AVCodec* codec = nullptr;

  if (!g_Config.sDumpEncoder.empty())
  {
    codec = avcodec_find_encoder_by_name(g_Config.sDumpEncoder.c_str());
    if (!codec)
      WARN_LOG(VIDEO, "Invalid encoder %s", g_Config.sDumpEncoder.c_str());
  }
  if (!codec)
    codec = avcodec_find_encoder(codec_id);

  s_codec_context = avcodec_alloc_context3(codec);
  if (!codec || !s_codec_context)
  {
    ERROR_LOG(VIDEO, "Could not find encoder or allocate codec context");
    return false;
  }

  // Force XVID FourCC for better compatibility when using H.263
  if (codec->id == AV_CODEC_ID_MPEG4)
    s_codec_context->codec_tag = MKTAG('X', 'V', 'I', 'D');

  s_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  s_codec_context->bit_rate = g_Config.iBitrateKbps * 1000;
  s_codec_context->width = s_width;
  s_codec_context->height = s_height;
  s_codec_context->time_base.num = 1;
  s_codec_context->time_base.den = VideoInterface::GetTargetRefreshRate();
  s_codec_context->gop_size = 1;
  s_codec_context->pix_fmt = g_Config.bUseFFV1 ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_YUV420P;

  if (output_format->flags & AVFMT_GLOBALHEADER)
    s_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (avcodec_open2(s_codec_context, codec, nullptr) < 0)
  {
    ERROR_LOG(VIDEO, "Could not open codec");
    return false;
  }

  s_src_frame = av_frame_alloc();
  s_scaled_frame = av_frame_alloc();

  s_scaled_frame->format = s_codec_context->pix_fmt;
  s_scaled_frame->width = s_width;
  s_scaled_frame->height = s_height;

#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (av_frame_get_buffer(s_scaled_frame, 1))
    return false;
#else
  if (avcodec_default_get_buffer(s_codec_context, s_scaled_frame))
    return false;
#endif

  s_stream = avformat_new_stream(s_format_context, codec);
  if (!s_stream || !AVStreamCopyContext(s_stream, s_codec_context))
  {
    ERROR_LOG(VIDEO, "Could not create stream");
    return false;
  }

  NOTICE_LOG(VIDEO, "Opening file %s for dumping", dump_path.c_str());
  if (avio_open(&s_format_context->pb, dump_path.c_str(), AVIO_FLAG_WRITE) < 0 ||
      avformat_write_header(s_format_context, nullptr))
  {
    ERROR_LOG(VIDEO, "Could not open %s", dump_path.c_str());
    return false;
  }

  OSD::AddMessage(
      StringFromFormat("Dumping Frames to \"%s\" (%dx%d)", dump_path.c_str(), s_width, s_height));

  return true;
}

static void PreparePacket(AVPacket* pkt)
{
  av_init_packet(pkt);
  pkt->data = nullptr;
  pkt->size = 0;
}

static int ReceivePacket(AVCodecContext* avctx, AVPacket* pkt, int* got_packet)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 37, 100)
  return avcodec_encode_video2(avctx, pkt, nullptr, got_packet);
#else
  *got_packet = 0;
  int error = avcodec_receive_packet(avctx, pkt);
  if (!error)
    *got_packet = 1;
  if (error == AVERROR(EAGAIN))
    return 0;

  return error;
#endif
}

static int SendFrameAndReceivePacket(AVCodecContext* avctx, AVPacket* pkt, AVFrame* frame,
                                     int* got_packet)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 37, 100)
  return avcodec_encode_video2(avctx, pkt, frame, got_packet);
#else
  *got_packet = 0;
  int error = avcodec_send_frame(avctx, frame);
  if (error)
    return error;

  return ReceivePacket(avctx, pkt, got_packet);
#endif
}

static void WritePacket(AVPacket& pkt)
{
  // Write the compressed frame in the media file.
  if (pkt.pts != (s64)AV_NOPTS_VALUE)
  {
    pkt.pts = av_rescale_q(pkt.pts, s_codec_context->time_base, s_stream->time_base);
  }
  if (pkt.dts != (s64)AV_NOPTS_VALUE)
  {
    pkt.dts = av_rescale_q(pkt.dts, s_codec_context->time_base, s_stream->time_base);
  }
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 60, 100)
  if (s_codec_context->coded_frame->key_frame)
    pkt.flags |= AV_PKT_FLAG_KEY;
#endif
  pkt.stream_index = s_stream->index;
  av_interleaved_write_frame(s_format_context, &pkt);
}

void FrameDump::AddFrame(const u8* data, int width, int height, int stride, const Frame& state)
{
  // Assume that the timing is valid, if the savestate id of the new frame
  // doesn't match the last one.
  if (state.savestate_index != s_last_savestate_index)
  {
    s_last_savestate_index = state.savestate_index;
    s_last_frame_is_valid = false;
  }

  CheckResolution(width, height);
  s_src_frame->data[0] = const_cast<u8*>(data);
  s_src_frame->linesize[0] = stride;
  s_src_frame->format = s_pix_fmt;
  s_src_frame->width = s_width;
  s_src_frame->height = s_height;

  // Convert image from {BGR24, RGBA} to desired pixel format
  s_sws_context =
      sws_getCachedContext(s_sws_context, width, height, s_pix_fmt, s_width, s_height,
                           s_codec_context->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
  if (s_sws_context)
  {
    sws_scale(s_sws_context, s_src_frame->data, s_src_frame->linesize, 0, height,
              s_scaled_frame->data, s_scaled_frame->linesize);
  }

  // Encode and write the image.
  AVPacket pkt;
  PreparePacket(&pkt);
  int got_packet = 0;
  int error = 0;
  u64 delta;
  s64 last_pts;
  // Check to see if the first frame being dumped is the first frame of output from the emulator.
  // This prevents an issue with starting dumping later in emulation from placing the frames
  // incorrectly.
  if (!s_last_frame_is_valid)
  {
    s_last_frame = state.ticks;
    s_last_frame_is_valid = true;
  }
  if (!s_start_dumping && state.first_frame)
  {
    delta = state.ticks;
    last_pts = AV_NOPTS_VALUE;
    s_start_dumping = true;
  }
  else
  {
    delta = state.ticks - s_last_frame;
    last_pts = (s_last_pts * s_codec_context->time_base.den) / state.ticks_per_second;
  }
  u64 pts_in_ticks = s_last_pts + delta;
  s_scaled_frame->pts = (pts_in_ticks * s_codec_context->time_base.den) / state.ticks_per_second;
  if (s_scaled_frame->pts != last_pts)
  {
    s_last_frame = state.ticks;
    s_last_pts = pts_in_ticks;
    error = SendFrameAndReceivePacket(s_codec_context, &pkt, s_scaled_frame, &got_packet);
  }
  if (!error && got_packet)
  {
    WritePacket(pkt);
  }
  if (error)
    ERROR_LOG(VIDEO, "Error while encoding video: %d", error);
}

static void HandleDelayedPackets()
{
  AVPacket pkt;

  while (true)
  {
    PreparePacket(&pkt);
    int got_packet;
    int error = ReceivePacket(s_codec_context, &pkt, &got_packet);
    if (error)
    {
      ERROR_LOG(VIDEO, "Error while stopping video: %d", error);
      break;
    }

    if (!got_packet)
      break;

    WritePacket(pkt);
  }
}

void FrameDump::Stop()
{
  HandleDelayedPackets();
  av_write_trailer(s_format_context);
  CloseVideoFile();
  s_file_index = 0;
  NOTICE_LOG(VIDEO, "Stopping frame dump");
  OSD::AddMessage("Stopped dumping frames");
}

void FrameDump::CloseVideoFile()
{
  av_frame_free(&s_src_frame);
  av_frame_free(&s_scaled_frame);

  avcodec_free_context(&s_codec_context);

  if (s_format_context)
  {
    avio_closep(&s_format_context->pb);
  }
  avformat_free_context(s_format_context);
  s_format_context = nullptr;

  if (s_sws_context)
  {
    sws_freeContext(s_sws_context);
    s_sws_context = nullptr;
  }
}

void FrameDump::DoState()
{
  s_savestate_index++;
}

void FrameDump::CheckResolution(int width, int height)
{
  // We check here to see if the requested width and height have changed since the last frame which
  // was dumped, then create a new file accordingly. However, is it possible for the height
  // (possibly width as well, but no examples known) to have a value of zero. This can occur as the
  // VI is able to be set to a zero value for height/width to disable output. If this is the case,
  // simply keep the last known resolution of the video for the added frame.
  if ((width != s_width || height != s_height) && (width > 0 && height > 0))
  {
    int temp_file_index = s_file_index;
    Stop();
    s_file_index = temp_file_index + 1;
    Start(width, height);
  }
}

FrameDump::Frame FrameDump::FetchState(u64 ticks)
{
  Frame state;
  state.ticks = ticks;
  state.first_frame = Movie::GetCurrentFrame() < 1;
  state.ticks_per_second = SystemTimers::GetTicksPerSecond();
  state.savestate_index = s_savestate_index;
  return state;
}
