// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(DYNAMICALLY_LOAD_FFMPEG)

#include "Common/FFmpegLoader.h"

#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "Common/DynamicLibrary.h"
#include "Common/Logging/Log.h"
#if defined(_WIN32)
#include "Common/FileUtil.h"
#endif

namespace
{
decltype(av_frame_alloc)* dl_av_frame_alloc;
decltype(av_frame_free)* dl_av_frame_free;
decltype(av_frame_get_buffer)* dl_av_frame_get_buffer;
decltype(av_get_pix_fmt)* dl_av_get_pix_fmt;
decltype(av_guess_format)* dl_av_guess_format;
decltype(av_interleaved_write_frame)* dl_av_interleaved_write_frame;
decltype(av_log_get_level)* dl_av_log_get_level;
decltype(av_log_set_callback)* dl_av_log_set_callback;
decltype(av_log_set_level)* dl_av_log_set_level;
decltype(av_opt_set_int)* dl_av_opt_set_int;
decltype(av_packet_alloc)* dl_av_packet_alloc;
decltype(av_packet_free)* dl_av_packet_free;
decltype(av_packet_rescale_ts)* dl_av_packet_rescale_ts;
decltype(av_reduce)* dl_av_reduce;
decltype(av_rescale_q)* dl_av_rescale_q;
decltype(av_strerror)* dl_av_strerror;
decltype(av_write_trailer)* dl_av_write_trailer;

decltype(avcodec_alloc_context3)* dl_avcodec_alloc_context3;
decltype(avcodec_descriptor_get_by_name)* dl_avcodec_descriptor_get_by_name;
decltype(avcodec_find_encoder_by_name)* dl_avcodec_find_encoder_by_name;
decltype(avcodec_find_encoder)* dl_avcodec_find_encoder;
decltype(avcodec_free_context)* dl_avcodec_free_context;
decltype(avcodec_open2)* dl_avcodec_open2;
decltype(avcodec_parameters_from_context)* dl_avcodec_parameters_from_context;
decltype(avcodec_receive_packet)* dl_avcodec_receive_packet;
decltype(avcodec_send_frame)* dl_avcodec_send_frame;

decltype(avformat_alloc_output_context2)* dl_avformat_alloc_output_context2;
decltype(avformat_free_context)* dl_avformat_free_context;
decltype(avformat_network_init)* dl_avformat_network_init;
decltype(avformat_new_stream)* dl_avformat_new_stream;
decltype(avformat_write_header)* dl_avformat_write_header;

decltype(avio_closep)* dl_avio_closep;
decltype(avio_open)* dl_avio_open;

decltype(sws_freeContext)* dl_sws_freeContext;
decltype(sws_getCachedContext)* dl_sws_getCachedContext;
decltype(sws_scale)* dl_sws_scale;

std::function<bool()> s_ffmpeg_load_failure_callback;
std::recursive_mutex s_init_mutex;

}  // namespace

namespace Common
{

std::string GetSharedFFmpegPath()
{
#if defined(_WIN32)
  return File::GetUserPath(D_LOAD_IDX) + "Library/";
#else
  return {};
#endif
}

void SetSharedFFmpegLoadFailureCallback(std::function<bool()> callback)
{
  s_ffmpeg_load_failure_callback = std::move(callback);
}

bool LoadSharedFFmpeg()
{
#if defined(_WIN32)
  constexpr auto avutil_filename = "avutil-61.dll";
  constexpr auto avcodec_filename = "avcodec-63.dll";
  constexpr auto avformat_filename = "avformat-63.dll";
  constexpr auto swscale_filename = "swscale-10.dll";
#else
  constexpr auto avutil_filename = "libavutil.so.61";
  constexpr auto avcodec_filename = "libavcodec.so.63";
  constexpr auto avformat_filename = "libavformat.so.63";
  constexpr auto swscale_filename = "libswscale.so.10";
#endif

  static Common::DynamicLibrary libavutil;
  static Common::DynamicLibrary libavcodec;
  static Common::DynamicLibrary libavformat;
  static Common::DynamicLibrary libswscale;

  static bool initialized = false;

  std::lock_guard lk{s_init_mutex};

  if (initialized)
    return true;

  const auto library_dir = GetSharedFFmpegPath();

  bool good = true;

  good &= libavutil.Open((library_dir + avutil_filename).c_str());
  good &= libavcodec.Open((library_dir + avcodec_filename).c_str());
  good &= libavformat.Open((library_dir + avformat_filename).c_str());
  good &= libswscale.Open((library_dir + swscale_filename).c_str());

  const auto handle_failure = [&] {
    libavutil.Close();
    libavcodec.Close();
    libavformat.Close();
    libswscale.Close();

    if (s_ffmpeg_load_failure_callback)
    {
      if (s_ffmpeg_load_failure_callback())
      {
        LoadSharedFFmpeg();
      }
    }
  };

  if (!good)
  {
    ERROR_LOG_FMT(COMMON, "Failed to open FFmpeg libraries ({})", library_dir);
    handle_failure();
    return initialized;
  }

  // clang-format off
  good &= libavutil.GetSymbol("av_frame_alloc", &dl_av_frame_alloc);
  good &= libavutil.GetSymbol("av_frame_free", &dl_av_frame_free);
  good &= libavutil.GetSymbol("av_frame_get_buffer", &dl_av_frame_get_buffer);
  good &= libavutil.GetSymbol("av_get_pix_fmt", &dl_av_get_pix_fmt);
  good &= libavformat.GetSymbol("av_guess_format", &dl_av_guess_format);
  good &= libavformat.GetSymbol("av_interleaved_write_frame", &dl_av_interleaved_write_frame);
  good &= libavutil.GetSymbol("av_log_get_level", &dl_av_log_get_level);
  good &= libavutil.GetSymbol("av_log_set_callback", &dl_av_log_set_callback);
  good &= libavutil.GetSymbol("av_log_set_level", &dl_av_log_set_level);
  good &= libavutil.GetSymbol("av_opt_set_int", &dl_av_opt_set_int);
  good &= libavcodec.GetSymbol("av_packet_alloc", &dl_av_packet_alloc);
  good &= libavcodec.GetSymbol("av_packet_free", &dl_av_packet_free);
  good &= libavcodec.GetSymbol("av_packet_rescale_ts", &dl_av_packet_rescale_ts);
  good &= libavutil.GetSymbol("av_reduce", &dl_av_reduce);
  good &= libavutil.GetSymbol("av_rescale_q", &dl_av_rescale_q);
  good &= libavutil.GetSymbol("av_strerror", &dl_av_strerror);
  good &= libavformat.GetSymbol("av_write_trailer", &dl_av_write_trailer);

  good &= libavcodec.GetSymbol("avcodec_alloc_context3", &dl_avcodec_alloc_context3);
  good &= libavcodec.GetSymbol("avcodec_descriptor_get_by_name", &dl_avcodec_descriptor_get_by_name);
  good &= libavcodec.GetSymbol("avcodec_find_encoder_by_name", &dl_avcodec_find_encoder_by_name);
  good &= libavcodec.GetSymbol("avcodec_find_encoder", &dl_avcodec_find_encoder);
  good &= libavcodec.GetSymbol("avcodec_free_context", &dl_avcodec_free_context);
  good &= libavcodec.GetSymbol("avcodec_open2", &dl_avcodec_open2);
  good &= libavcodec.GetSymbol("avcodec_parameters_from_context", &dl_avcodec_parameters_from_context);
  good &= libavcodec.GetSymbol("avcodec_receive_packet", &dl_avcodec_receive_packet);
  good &= libavcodec.GetSymbol("avcodec_send_frame", &dl_avcodec_send_frame);

  good &= libavformat.GetSymbol("avformat_alloc_output_context2", &dl_avformat_alloc_output_context2);
  good &= libavformat.GetSymbol("avformat_free_context", &dl_avformat_free_context);
  good &= libavformat.GetSymbol("avformat_network_init", &dl_avformat_network_init);
  good &= libavformat.GetSymbol("avformat_new_stream", &dl_avformat_new_stream);
  good &= libavformat.GetSymbol("avformat_write_header", &dl_avformat_write_header);

  good &= libavformat.GetSymbol("avio_closep", &dl_avio_closep);
  good &= libavformat.GetSymbol("avio_open", &dl_avio_open);

  good &= libswscale.GetSymbol("sws_freeContext", &dl_sws_freeContext);
  good &= libswscale.GetSymbol("sws_getCachedContext", &dl_sws_getCachedContext);
  good &= libswscale.GetSymbol("sws_scale", &dl_sws_scale);
  // clang-format on

  if (!good)
  {
    ERROR_LOG_FMT(COMMON, "Failed to load symbols from FFmpeg libraries ({})", library_dir);
    handle_failure();
    return initialized;
  }

  initialized = true;
  INFO_LOG_FMT(COMMON, "Dynamically loaded FFmpeg.");
  return true;
}

}  // namespace Common

void av_log_set_level(int level)
{
  dl_av_log_set_level(level);
}

int av_log_get_level()
{
  return dl_av_log_get_level();
}

void av_log_set_callback(void (*callback)(void*, int, const char*, va_list))
{
  dl_av_log_set_callback(callback);
}

int avformat_network_init()
{
  return dl_avformat_network_init();
}

AVPacket* av_packet_alloc()
{
  return dl_av_packet_alloc();
}

int av_opt_set_int(void* obj, const char* name, int64_t val, int search_flags)
{
  return dl_av_opt_set_int(obj, name, val, search_flags);
}

const AVOutputFormat* av_guess_format(const char* short_name, const char* filename,
                                      const char* mime_type)
{
  return dl_av_guess_format(short_name, filename, mime_type);
}

int avformat_alloc_output_context2(AVFormatContext** ctx, const AVOutputFormat* oformat,
                                   const char* format_name, const char* filename)
{
  return dl_avformat_alloc_output_context2(ctx, oformat, format_name, filename);
}

int avcodec_open2(AVCodecContext* avctx, const AVCodec* codec, AVDictionary** options)
{
  return dl_avcodec_open2(avctx, codec, options);
}

const AVCodecDescriptor* avcodec_descriptor_get_by_name(const char* name)
{
  return dl_avcodec_descriptor_get_by_name(name);
}

AVCodecContext* avcodec_alloc_context3(const AVCodec* codec)
{
  return dl_avcodec_alloc_context3(codec);
}

int av_frame_get_buffer(AVFrame* frame, int align)
{
  return dl_av_frame_get_buffer(frame, align);
}

AVStream* avformat_new_stream(AVFormatContext* s, const AVCodec* c)
{
  return dl_avformat_new_stream(s, c);
}

int avio_open(AVIOContext** s, const char* url, int flags)
{
  return dl_avio_open(s, url, flags);
}

void avformat_free_context(AVFormatContext* s)
{
  dl_avformat_free_context(s);
}

void av_frame_free(AVFrame** frame)
{
  dl_av_frame_free(frame);
}

const AVCodec* avcodec_find_encoder_by_name(const char* name)
{
  return dl_avcodec_find_encoder_by_name(name);
}

SwsContext* sws_getCachedContext(SwsContext* context, int srcW, int srcH, AVPixelFormat srcFormat,
                                 int dstW, int dstH, AVPixelFormat dstFormat, int flags,
                                 SwsFilter* srcFilter, SwsFilter* dstFilter, const double* param)
{
  return dl_sws_getCachedContext(context, srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags,
                                 srcFilter, dstFilter, param);
}

AVPixelFormat av_get_pix_fmt(const char* name)
{
  return dl_av_get_pix_fmt(name);
}

const AVCodec* avcodec_find_encoder(AVCodecID id)
{
  return dl_avcodec_find_encoder(id);
}

int avcodec_send_frame(AVCodecContext* avctx, const AVFrame* frame)
{
  return dl_avcodec_send_frame(avctx, frame);
}

int avcodec_parameters_from_context(AVCodecParameters* par, const AVCodecContext* codec)
{
  return dl_avcodec_parameters_from_context(par, codec);
}

int avformat_write_header(AVFormatContext* s, AVDictionary** options)
{
  return dl_avformat_write_header(s, options);
}

void avcodec_free_context(AVCodecContext** avctx)
{
  dl_avcodec_free_context(avctx);
}

int avio_closep(AVIOContext** s)
{
  return dl_avio_closep(s);
}

void sws_freeContext(SwsContext* swsContext)
{
  dl_sws_freeContext(swsContext);
}

int av_reduce(int* dst_num, int* dst_den, int64_t num, int64_t den, int64_t max)
{
  return dl_av_reduce(dst_num, dst_den, num, den, max);
}

int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq)
{
  return dl_av_rescale_q(a, bq, cq);
}

int sws_scale(SwsContext* c, const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY,
              int srcSliceH, uint8_t* const dst[], const int dstStride[])
{
  return dl_sws_scale(c, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
}

void av_packet_rescale_ts(AVPacket* pkt, AVRational tb_src, AVRational tb_dst)
{
  dl_av_packet_rescale_ts(pkt, tb_src, tb_dst);
}

int avcodec_receive_packet(AVCodecContext* avctx, AVPacket* avpkt)
{
  return dl_avcodec_receive_packet(avctx, avpkt);
}

AVFrame* av_frame_alloc()
{
  return dl_av_frame_alloc();
}

int av_interleaved_write_frame(AVFormatContext* s, AVPacket* pkt)
{
  return dl_av_interleaved_write_frame(s, pkt);
}

int av_write_trailer(AVFormatContext* s)
{
  return dl_av_write_trailer(s);
}

void av_packet_free(AVPacket** pkt)
{
  dl_av_packet_free(pkt);
}

int av_strerror(int errnum, char* errbuf, size_t errbuf_size)
{
  return dl_av_strerror(errnum, errbuf, errbuf_size);
}

#endif
