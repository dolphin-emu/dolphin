// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FrameDumpFFMpeg.h"
#include "Common/TimeUtil.h"

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif

#include <array>
#include <string>
#include <string_view>

#include <fmt/chrono.h>
#include <fmt/format.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/stereo3d.h>
#include <libswscale/swscale.h>
}

#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/System.h"

#include "VideoCommon/OnScreenDisplay.h"

struct FrameDumpContext
{
  AVFormatContext* format = nullptr;
  AVStream* stream = nullptr;
  AVCodecContext* codec = nullptr;
  AVFrame* src_frame = nullptr;
  AVFrame* scaled_frame = nullptr;
  SwsContext* sws = nullptr;

  s64 last_pts = AV_NOPTS_VALUE;

  int width = 0;
  int height = 0;
  FrameLayout layout = FrameLayout::Mono;

  u64 start_ticks = 0;
  u32 savestate_index = 0;

  bool gave_vfr_warning = false;
};

namespace
{
AVRational GetTimeBaseForCurrentRefreshRate(s64 max_denominator)
{
  // TODO: GetTargetRefreshRate* are not safe from GPU thread.
  auto& vi = Core::System::GetInstance().GetVideoInterface();
  int num;
  int den;
  av_reduce(&num, &den, int(vi.GetTargetRefreshRateDenominator()),
            int(vi.GetTargetRefreshRateNumerator()), max_denominator);
  return AVRational{num, den};
}

void InitAVCodec()
{
  static bool first_run = true;
  if (first_run)
  {
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback([](void* ptr, int level, const char* fmt, va_list vl) {
      if (level < 0)
        level = AV_LOG_DEBUG;
      if (level >= 0)
        level &= 0xff;

      if (level > av_log_get_level())
        return;

      auto log_level = Common::Log::LogLevel::LNOTICE;
      if (level >= AV_LOG_ERROR && level < AV_LOG_WARNING)
        log_level = Common::Log::LogLevel::LERROR;
      else if (level >= AV_LOG_WARNING && level < AV_LOG_INFO)
        log_level = Common::Log::LogLevel::LWARNING;
      else if (level >= AV_LOG_INFO && level < AV_LOG_DEBUG)
        log_level = Common::Log::LogLevel::LINFO;
      else if (level >= AV_LOG_DEBUG)
        // keep libav debug messages visible in release build of dolphin
        log_level = Common::Log::LogLevel::LINFO;

      // Don't perform this formatting if the log level is disabled
      auto* log_manager = Common::Log::LogManager::GetInstance();
      if (log_manager != nullptr &&
          log_manager->IsEnabled(Common::Log::LogType::FRAMEDUMP, log_level))
      {
        constexpr size_t MAX_MSGLEN = 1024;
        char message[MAX_MSGLEN];
        CharArrayFromFormatV(message, MAX_MSGLEN, fmt, vl);

        GENERIC_LOG_FMT(Common::Log::LogType::FRAMEDUMP, log_level, "{}", message);
      }
    });

    // TODO: We never call avformat_network_deinit.
    avformat_network_init();

    first_run = false;
  }
}

std::string GetDumpPath(const std::string& extension, std::time_t time, u32 index)
{
  const std::string dump_path = Config::Get(Config::GFX_DUMP_PATH);
  if (!dump_path.empty())
    return dump_path;

  const auto local_time = Common::LocalTime(time);
  if (!local_time)
    return "";

  const std::string path_prefix =
      File::GetUserPath(D_DUMPFRAMES_IDX) + SConfig::GetInstance().GetGameID();

  const std::string base_name =
      fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}_{}", path_prefix, *local_time, index);

  const std::string path = fmt::format("{}.{}", base_name, extension);

  // Ask to delete file.
  if (File::Exists(path))
  {
    if (Config::Get(Config::MAIN_MOVIE_DUMP_FRAMES_SILENT) ||
        AskYesNoFmtT("Delete the existing file '{0}'?", path))
    {
      File::Delete(path);
    }
    else
    {
      // Stop and cancel dumping the video
      return "";
    }
  }

  return path;
}

// const since libavformat 59.
using OutputFormat = decltype(av_guess_format(nullptr, nullptr, nullptr));

// Finds the container for a format the same way as for the file that GetDumpPath() names after it:
// by format name, else by file name extension (like "mkv" for Matroska).
OutputFormat GuessOutputFormat(const std::string& format)
{
  std::string file_name = Config::Get(Config::GFX_DUMP_PATH);
  if (file_name.empty())
    file_name = "dump." + format;
  return av_guess_format(format.c_str(), file_name.c_str(), nullptr);
}

std::string AVErrorString(int error)
{
  std::array<char, AV_ERROR_MAX_STRING_SIZE> msg;
  av_make_error_string(&msg[0], msg.size(), error);
  return fmt::format("{:8x} {}", (u32)error, &msg[0]);
}

// Stereoscopic 3D metadata for frames holding the views of both eyes next to each other, the left
// eye first. AVStereo3D doesn't tell full resolution views (which Dolphin dumps) from half
// resolution ones.
void SetSideBySideStereo3D(AVStereo3D* stereo3d)
{
  stereo3d->type = AV_STEREO3D_SIDEBYSIDE;
  stereo3d->flags = 0;
}

// Adds the stereoscopic 3D metadata to the stream, for muxers that store it in the container, like
// Matroska (StereoMode element) and MP4 ('st3d' box).
bool AddSideBySideStereo3DToStream(AVStream* stream)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(59, 26, 100)
  size_t size = 0;
  AVStereo3D* const stereo3d = av_stereo3d_alloc_size(&size);
#else
  const size_t size = sizeof(AVStereo3D);
  AVStereo3D* const stereo3d = av_stereo3d_alloc();
#endif
  if (!stereo3d)
    return false;

  SetSideBySideStereo3D(stereo3d);

  // Ownership of the side data is only transferred on success.
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 30, 100)
  AVCodecParameters* const codecpar = stream->codecpar;
  if (!av_packet_side_data_add(&codecpar->coded_side_data, &codecpar->nb_coded_side_data,
                               AV_PKT_DATA_STEREO3D, stereo3d, size, 0))
#else
  if (av_stream_add_side_data(stream, AV_PKT_DATA_STEREO3D, reinterpret_cast<uint8_t*>(stereo3d),
                              size) < 0)
#endif
  {
    av_free(stereo3d);
    return false;
  }
  return true;
}

}  // namespace

bool FFMpegFrameDump::Start(int w, int h, FrameLayout layout, u64 start_ticks)
{
  if (IsStarted())
    return true;

  m_savestate_index = 0;
  m_start_time = std::time(nullptr);
  m_file_index = 0;

  return PrepareEncoding(w, h, layout, start_ticks, m_savestate_index);
}

bool FFMpegFrameDump::PrepareEncoding(int w, int h, FrameLayout layout, u64 start_ticks,
                                      u32 savestate_index)
{
  m_context = std::make_unique<FrameDumpContext>();

  m_context->width = w;
  m_context->height = h;
  m_context->layout = layout;

  m_context->start_ticks = start_ticks;
  m_context->savestate_index = savestate_index;

  InitAVCodec();
  const bool success = CreateVideoFile();
  if (!success)
  {
    CloseVideoFile();
    OSD::AddMessage("FrameDump Start failed");
  }
  return success;
}

bool FFMpegFrameDump::CreateVideoFile()
{
  std::string format = Config::Get(Config::GFX_DUMP_FORMAT);

  OutputFormat output_format = GuessOutputFormat(format);
  if (!output_format)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Invalid format {}", format);
    return false;
  }

  // Without a configured codec, use the default one of the configured format, even if the
  // container changes below.
  AVCodecID codec_id = output_format->video_codec;

  // Video players and 3D displays only show videos in 3D when the file says that they are
  // stereoscopic, which AVI files can't, so use MKV files for those instead.
  const bool is_stereo = m_context->layout == FrameLayout::SideBySide;
  bool avi_replaced = false;
  if (is_stereo && std::string_view(output_format->name) == "avi")
  {
    if (const OutputFormat matroska = av_guess_format("matroska", nullptr, nullptr))
    {
      output_format = matroska;
      format = "mkv";
      avi_replaced = true;
    }
    else
    {
      WARN_LOG_FMT(FRAMEDUMP, "Can't write MKV files, so the stereoscopic 3D frame dump is an AVI "
                              "file, which video players can't tell from a 2D video");
    }
  }

  const std::string dump_path = GetDumpPath(format, m_start_time, m_file_index);

  if (dump_path.empty())
    return false;

  File::CreateFullPath(dump_path);

  if (avformat_alloc_output_context2(&m_context->format, output_format, nullptr,
                                     dump_path.c_str()) < 0)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not allocate output context");
    return false;
  }

  const std::string codec_name =
      Config::Get(Config::GFX_USE_LOSSLESS) ? "utvideo" : Config::Get(Config::GFX_DUMP_CODEC);

  if (!codec_name.empty())
  {
    const AVCodecDescriptor* const codec_desc = avcodec_descriptor_get_by_name(codec_name.c_str());
    if (codec_desc)
      codec_id = codec_desc->id;
    else
      WARN_LOG_FMT(FRAMEDUMP, "Invalid codec {}", codec_name);
  }

  const AVCodec* codec = nullptr;
  const std::string dump_encoder = Config::Get(Config::GFX_DUMP_ENCODER);
  if (!dump_encoder.empty())
  {
    codec = avcodec_find_encoder_by_name(dump_encoder.c_str());
    if (!codec)
      WARN_LOG_FMT(FRAMEDUMP, "Invalid encoder {}", dump_encoder);
  }
  if (!codec)
    codec = avcodec_find_encoder(codec_id);

  m_context->codec = avcodec_alloc_context3(codec);
  if (!codec || !m_context->codec)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not find encoder or allocate codec context");
    return false;
  }

  m_max_denominator = std::numeric_limits<s64>::max();

  if (codec->id == AV_CODEC_ID_MPEG4)
  {
    // Force XVID FourCC for better compatibility when using H.263, unless the container doesn't
    // accept it, like MP4.
    constexpr unsigned int xvid_tag = MKTAG('X', 'V', 'I', 'D');
    if (av_codec_get_id(output_format->codec_tag, xvid_tag) == AV_CODEC_ID_MPEG4)
      m_context->codec->codec_tag = xvid_tag;
    m_max_denominator = std::numeric_limits<unsigned short>::max();

    // Only for MPEG-4 Part 2, as other encoders like libx264 refuse to open with this level.
    m_context->codec->level = 1;
  }

  const auto time_base = GetTimeBaseForCurrentRefreshRate(m_max_denominator);

  INFO_LOG_FMT(FRAMEDUMP, "Creating video file: {} x {} @ {}/{} fps", m_context->width,
               m_context->height, time_base.den, time_base.num);

  m_context->codec->codec_type = AVMEDIA_TYPE_VIDEO;
  m_context->codec->bit_rate = static_cast<int64_t>(Config::Get(Config::GFX_BITRATE_KBPS)) * 1000;
  m_context->codec->width = m_context->width;
  m_context->codec->height = m_context->height;
  m_context->codec->time_base = time_base;
  m_context->codec->gop_size = 1;

  AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

  const std::string pixel_format_string = Config::Get(Config::GFX_DUMP_PIXEL_FORMAT);
  if (!pixel_format_string.empty())
  {
    pix_fmt = av_get_pix_fmt(pixel_format_string.c_str());
    if (pix_fmt == AV_PIX_FMT_NONE)
      WARN_LOG_FMT(FRAMEDUMP, "Invalid pixel format {}", pixel_format_string);
  }

  if (pix_fmt == AV_PIX_FMT_NONE)
  {
    if (m_context->codec->codec_id == AV_CODEC_ID_FFV1)
      pix_fmt = AV_PIX_FMT_BGR0;
    else if (m_context->codec->codec_id == AV_CODEC_ID_UTVIDEO)
      pix_fmt = AV_PIX_FMT_GBRP;
    else
      pix_fmt = AV_PIX_FMT_YUV420P;
  }

  m_context->codec->pix_fmt = pix_fmt;

  if (m_context->codec->codec_id == AV_CODEC_ID_UTVIDEO)
    av_opt_set_int(m_context->codec->priv_data, "pred", 3, 0);  // median

  if (output_format->flags & AVFMT_GLOBALHEADER)
    m_context->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (avcodec_open2(m_context->codec, codec, nullptr) < 0)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not open codec");
    return false;
  }

  m_context->src_frame = av_frame_alloc();
  m_context->scaled_frame = av_frame_alloc();

  m_context->scaled_frame->format = m_context->codec->pix_fmt;
  m_context->scaled_frame->width = m_context->width;
  m_context->scaled_frame->height = m_context->height;

  if (av_frame_get_buffer(m_context->scaled_frame, 1))
    return false;

  // The frame is reused for every frame sent to the encoder, so the stereoscopic 3D metadata only
  // needs to be attached once. Encoders that support it signal it in the video stream itself,
  // like libx264 with a frame packing arrangement SEI message.
  if (is_stereo)
  {
    AVStereo3D* const stereo3d = av_stereo3d_create_side_data(m_context->scaled_frame);
    if (!stereo3d)
    {
      ERROR_LOG_FMT(FRAMEDUMP, "Could not allocate stereoscopic 3D frame side data");
      return false;
    }
    SetSideBySideStereo3D(stereo3d);
  }

  m_context->stream = avformat_new_stream(m_context->format, codec);
  if (!m_context->stream ||
      avcodec_parameters_from_context(m_context->stream->codecpar, m_context->codec) < 0)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not create stream");
    return false;
  }

  m_context->stream->time_base = m_context->codec->time_base;

  if (is_stereo)
  {
    if (!AddSideBySideStereo3DToStream(m_context->stream))
    {
      ERROR_LOG_FMT(FRAMEDUMP, "Could not add stereoscopic 3D stream side data");
      return false;
    }

    // The MP4 muxer only writes its 'st3d' box when unofficial extensions are allowed.
    m_context->format->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
  }

  NOTICE_LOG_FMT(FRAMEDUMP, "Opening file {} for dumping", dump_path);
  if (avio_open(&m_context->format->pb, dump_path.c_str(), AVIO_FLAG_WRITE) < 0 ||
      avformat_write_header(m_context->format, nullptr))
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not open {}", dump_path);
    return false;
  }

  if (av_cmp_q(m_context->stream->time_base, time_base) != 0)
  {
    WARN_LOG_FMT(FRAMEDUMP, "Stream time base differs at {}/{}", m_context->stream->time_base.den,
                 m_context->stream->time_base.num);
  }

  if (avi_replaced)
  {
    NOTICE_LOG_FMT(FRAMEDUMP, "Dumping frames to MKV instead of AVI, which can't be marked as "
                              "stereoscopic 3D");
    OSD::AddMessage("Dumping frames to MKV instead of AVI, which can't be marked as 3D");
  }
  OSD::AddMessage(fmt::format("Dumping Frames to \"{}\" ({}x{}{})", dump_path, m_context->width,
                              m_context->height, is_stereo ? ", side-by-side 3D" : ""));
  return true;
}

bool FFMpegFrameDump::IsFirstFrameInCurrentFile() const
{
  return m_context->last_pts == AV_NOPTS_VALUE;
}

void FFMpegFrameDump::AddFrame(const FrameData& frame)
{
  // Are we even dumping?
  if (!IsStarted())
    return;

  CheckForConfigChange(frame);

  // Handle failure after a config change.
  if (!IsStarted())
    return;

  // Calculate presentation timestamp from ticks since start.
  const s64 pts = av_rescale_q(
      frame.state.ticks - m_context->start_ticks,
      // TODO: GetTicksPerSecond is not safe from GPU thread.
      AVRational{1, int(Core::System::GetInstance().GetSystemTimers().GetTicksPerSecond())},
      m_context->codec->time_base);

  if (!IsFirstFrameInCurrentFile())
  {
    if (pts <= m_context->last_pts)
    {
      WARN_LOG_FMT(FRAMEDUMP, "PTS delta < 1. Current frame will not be dumped.");
      return;
    }
    else if (pts > m_context->last_pts + 1 && !m_context->gave_vfr_warning)
    {
      WARN_LOG_FMT(FRAMEDUMP, "PTS delta > 1. Resulting file will have variable frame rate. "
                              "Subsequent occurrences will not be reported.");
      m_context->gave_vfr_warning = true;
    }
  }

  constexpr AVPixelFormat pix_fmt = AV_PIX_FMT_RGBA;

  m_context->src_frame->data[0] = const_cast<u8*>(frame.data);
  m_context->src_frame->linesize[0] = frame.stride;
  m_context->src_frame->format = pix_fmt;
  m_context->src_frame->width = m_context->width;
  m_context->src_frame->height = m_context->height;

  // Convert image from RGBA to desired pixel format.
  m_context->sws = sws_getCachedContext(
      m_context->sws, frame.width, frame.height, pix_fmt, m_context->width, m_context->height,
      m_context->codec->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
  if (m_context->sws)
  {
    sws_scale(m_context->sws, m_context->src_frame->data, m_context->src_frame->linesize, 0,
              frame.height, m_context->scaled_frame->data, m_context->scaled_frame->linesize);
  }

  m_context->last_pts = pts;
  m_context->scaled_frame->pts = pts;

  if (const int error = avcodec_send_frame(m_context->codec, m_context->scaled_frame))
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Error while encoding video: {}", AVErrorString(error));
    return;
  }

  ProcessPackets();
}

void FFMpegFrameDump::ProcessPackets()
{
  auto pkt = std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>(
      av_packet_alloc(), [](AVPacket* packet) { av_packet_free(&packet); });

  if (!pkt)
  {
    ERROR_LOG_FMT(FRAMEDUMP, "Could not allocate packet");
    return;
  }

  while (true)
  {
    const int receive_error = avcodec_receive_packet(m_context->codec, pkt.get());

    if (receive_error == AVERROR(EAGAIN) || receive_error == AVERROR_EOF)
    {
      // We have processed all available packets.
      break;
    }

    if (receive_error)
    {
      ERROR_LOG_FMT(FRAMEDUMP, "Error receiving packet: {}", AVErrorString(receive_error));
      break;
    }

    av_packet_rescale_ts(pkt.get(), m_context->codec->time_base, m_context->stream->time_base);
    pkt->stream_index = m_context->stream->index;

    if (const int write_error = av_interleaved_write_frame(m_context->format, pkt.get()))
    {
      ERROR_LOG_FMT(FRAMEDUMP, "Error writing packet: {}", AVErrorString(write_error));
      break;
    }
  }
}

void FFMpegFrameDump::Stop()
{
  if (!IsStarted())
    return;

  // Signal end of stream to encoder.
  if (const int flush_error = avcodec_send_frame(m_context->codec, nullptr))
    WARN_LOG_FMT(FRAMEDUMP, "Error sending flush packet: {}", AVErrorString(flush_error));

  ProcessPackets();
  av_write_trailer(m_context->format);
  CloseVideoFile();

  NOTICE_LOG_FMT(FRAMEDUMP, "Stopping frame dump");
  OSD::AddMessage("Stopped dumping frames");
}

bool FFMpegFrameDump::IsStarted() const
{
  return m_context != nullptr;
}

void FFMpegFrameDump::CloseVideoFile()
{
  av_frame_free(&m_context->src_frame);
  av_frame_free(&m_context->scaled_frame);

  avcodec_free_context(&m_context->codec);

  if (m_context->format)
    avio_closep(&m_context->format->pb);

  avformat_free_context(m_context->format);

  if (m_context->sws)
    sws_freeContext(m_context->sws);

  m_context.reset();
}

void FFMpegFrameDump::DoState(PointerWrap& p)
{
  if (p.IsReadMode())
    ++m_savestate_index;
}

void FFMpegFrameDump::CheckForConfigChange(const FrameData& frame)
{
  bool restart_dump = false;

  // We check here to see if the requested width and height have changed since the last frame which
  // was dumped, then create a new file accordingly. However, is it possible for the height
  // (possibly width as well, but no examples known) to have a value of zero. This can occur as the
  // VI is able to be set to a zero value for height/width to disable output. If this is the case,
  // simply keep the last known resolution of the video for the added frame.
  if ((frame.width != m_context->width || frame.height != m_context->height) &&
      (frame.width > 0 && frame.height > 0))
  {
    INFO_LOG_FMT(FRAMEDUMP, "Starting new dump on resolution change.");
    restart_dump = true;
  }
  else if (frame.layout != m_context->layout)
  {
    INFO_LOG_FMT(FRAMEDUMP, "Starting new dump on stereoscopic 3D change.");
    restart_dump = true;
  }
  else if (!IsFirstFrameInCurrentFile() &&
           frame.state.savestate_index != m_context->savestate_index)
  {
    INFO_LOG_FMT(FRAMEDUMP, "Starting new dump on savestate load.");
    restart_dump = true;
  }
  else if (frame.state.refresh_rate_den != m_context->codec->time_base.num ||
           frame.state.refresh_rate_num != m_context->codec->time_base.den)
  {
    INFO_LOG_FMT(FRAMEDUMP, "Starting new dump on refresh rate change {}/{} vs {}/{}.",
                 m_context->codec->time_base.den, m_context->codec->time_base.num,
                 frame.state.refresh_rate_num, frame.state.refresh_rate_den);
    restart_dump = true;
  }

  if (restart_dump)
  {
    Stop();
    ++m_file_index;
    PrepareEncoding(frame.width, frame.height, frame.layout, frame.state.ticks,
                    frame.state.savestate_index);
  }
}

FrameState FFMpegFrameDump::FetchState(u64 ticks, int frame_number) const
{
  FrameState state;
  state.ticks = ticks;
  state.frame_number = frame_number;
  state.savestate_index = m_savestate_index;

  const auto time_base = GetTimeBaseForCurrentRefreshRate(m_max_denominator);
  state.refresh_rate_num = time_base.den;
  state.refresh_rate_den = time_base.num;
  return state;
}

FFMpegFrameDump::FFMpegFrameDump() = default;

FFMpegFrameDump::~FFMpegFrameDump()
{
  Stop();
}
