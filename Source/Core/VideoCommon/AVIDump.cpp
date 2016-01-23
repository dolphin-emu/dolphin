// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
}

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h" //for TargetRefreshRate
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/VideoConfig.h"

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

static AVFormatContext* s_format_context = nullptr;
static AVStream* s_stream = nullptr;
static AVFrame* s_src_frame = nullptr;
static AVFrame* s_scaled_frame = nullptr;
static uint8_t* s_yuv_buffer = nullptr;
static SwsContext* s_sws_context = nullptr;
static int s_width;
static int s_height;
static int s_size;
static u64 s_last_frame;
static bool s_start_dumping = false;
static u64 s_last_pts;

static void InitAVCodec()
{
	static bool first_run = true;
	if (first_run)
	{
		av_register_all();
		first_run = false;
	}
}

bool AVIDump::Start(int w, int h)
{
	s_width = w;
	s_height = h;

	s_last_frame = CoreTiming::GetTicks();
	s_last_pts = 0;

	InitAVCodec();
	bool success = CreateFile();
	if (!success)
		CloseFile();
	return success;
}

bool AVIDump::CreateFile()
{
	AVCodec* codec = nullptr;

	s_format_context = avformat_alloc_context();
	snprintf(s_format_context->filename, sizeof(s_format_context->filename), "%s",
	         (File::GetUserPath(D_DUMPFRAMES_IDX) + "framedump0.avi").c_str());
	File::CreateFullPath(s_format_context->filename);

	// Ask to delete file
	if (File::Exists(s_format_context->filename))
	{
		if (SConfig::GetInstance().m_DumpFramesSilent ||
			AskYesNoT("Delete the existing file '%s'?", s_format_context->filename))
		{
			File::Delete(s_format_context->filename);
		}
		else
		{
			// Stop and cancel dumping the video
			return false;
		}
	}

	if (!(s_format_context->oformat = av_guess_format("avi", nullptr, nullptr)) ||
	    !(s_stream = avformat_new_stream(s_format_context, codec)))
	{
		return false;
	}

	s_stream->codec->codec_id = g_Config.bUseFFV1 ? AV_CODEC_ID_FFV1
	                                              : s_format_context->oformat->video_codec;
	if (!g_Config.bUseFFV1)
		s_stream->codec->codec_tag = MKTAG('X', 'V', 'I', 'D'); // Force XVID FourCC for better compatibility
	s_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	s_stream->codec->bit_rate = 400000;
	s_stream->codec->width = s_width;
	s_stream->codec->height = s_height;
	s_stream->codec->time_base.num = 1;
	s_stream->codec->time_base.den = VideoInterface::GetTargetRefreshRate();
	s_stream->codec->gop_size = 12;
	s_stream->codec->pix_fmt = g_Config.bUseFFV1 ? AV_PIX_FMT_BGRA : AV_PIX_FMT_YUV420P;

	if (!(codec = avcodec_find_encoder(s_stream->codec->codec_id)) ||
	    (avcodec_open2(s_stream->codec, codec, nullptr) < 0))
	{
		return false;
	}

	s_src_frame = av_frame_alloc();
	s_scaled_frame = av_frame_alloc();

	s_size = avpicture_get_size(s_stream->codec->pix_fmt, s_width, s_height);

	s_yuv_buffer = new uint8_t[s_size];
	avpicture_fill((AVPicture*)s_scaled_frame, s_yuv_buffer, s_stream->codec->pix_fmt, s_width, s_height);

	NOTICE_LOG(VIDEO, "Opening file %s for dumping", s_format_context->filename);
	if (avio_open(&s_format_context->pb, s_format_context->filename, AVIO_FLAG_WRITE) < 0)
	{
		WARN_LOG(VIDEO, "Could not open %s", s_format_context->filename);
		return false;
	}

	avformat_write_header(s_format_context, nullptr);

	return true;
}

static void PreparePacket(AVPacket* pkt)
{
	av_init_packet(pkt);
	pkt->data = nullptr;
	pkt->size = 0;
}

void AVIDump::AddFrame(const u8* data, int width, int height)
{
	avpicture_fill((AVPicture*)s_src_frame, const_cast<u8*>(data), AV_PIX_FMT_BGR24, width, height);

	// Convert image from BGR24 to desired pixel format, and scale to initial
	// width and height
	if ((s_sws_context = sws_getCachedContext(s_sws_context,
	                                          width, height, AV_PIX_FMT_BGR24,
	                                          s_width, s_height, s_stream->codec->pix_fmt,
	                                          SWS_BICUBIC, nullptr, nullptr, nullptr)))
	{
		sws_scale(s_sws_context, s_src_frame->data, s_src_frame->linesize, 0,
		          height, s_scaled_frame->data, s_scaled_frame->linesize);
	}

	s_scaled_frame->format = s_stream->codec->pix_fmt;
	s_scaled_frame->width = s_width;
	s_scaled_frame->height = s_height;

	// Encode and write the image.
	AVPacket pkt;
	PreparePacket(&pkt);
	int got_packet = 0;
	int error = 0;
	u64 delta;
	s64 last_pts;
	if (!s_start_dumping && s_last_frame <= (SystemTimers::GetTicksPerSecond() * 2)) // Hueristic to catch delay from power on to adding the first frame to video
	{
		delta = CoreTiming::GetTicks();
		last_pts = AV_NOPTS_VALUE;
		s_start_dumping = true;
	}
	else
	{
		delta = CoreTiming::GetTicks() - s_last_frame;
		last_pts = (s_last_pts * s_stream->codec->time_base.den) / SystemTimers::GetTicksPerSecond();
	}
	u64 pts_in_ticks = s_last_pts + delta;
	s_scaled_frame->pts = (pts_in_ticks * s_stream->codec->time_base.den) / SystemTimers::GetTicksPerSecond();
	if (s_scaled_frame->pts != last_pts)
	{
		s_last_frame = CoreTiming::GetTicks();
		s_last_pts = pts_in_ticks;
		error = avcodec_encode_video2(s_stream->codec, &pkt, s_scaled_frame, &got_packet);
	}
	while (!error && got_packet)
	{
		// Write the compressed frame in the media file.
		if (pkt.pts != (s64)AV_NOPTS_VALUE)
		{
			pkt.pts = av_rescale_q(pkt.pts,
			                       s_stream->codec->time_base, s_stream->time_base);
		}
		if (pkt.dts != (s64)AV_NOPTS_VALUE)
		{
			pkt.dts = av_rescale_q(pkt.dts,
			                       s_stream->codec->time_base, s_stream->time_base);
		}
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 60, 100)
		if (s_stream->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
#endif
		pkt.stream_index = s_stream->index;
		av_interleaved_write_frame(s_format_context, &pkt);

		// Handle delayed frames.
		PreparePacket(&pkt);
		error = avcodec_encode_video2(s_stream->codec, &pkt, nullptr, &got_packet);
	}
	if (error)
		ERROR_LOG(VIDEO, "Error while encoding video: %d", error);
}

void AVIDump::Stop()
{
	av_write_trailer(s_format_context);
	CloseFile();
	NOTICE_LOG(VIDEO, "Stopping frame dump");
}

void AVIDump::CloseFile()
{
	if (s_stream)
	{
		if (s_stream->codec)
			avcodec_close(s_stream->codec);
		av_free(s_stream);
		s_stream = nullptr;
	}

	if (s_yuv_buffer)
	{
		delete[] s_yuv_buffer;
		s_yuv_buffer = nullptr;
	}

	av_frame_free(&s_src_frame);
	av_frame_free(&s_scaled_frame);

	if (s_format_context)
	{
		if (s_format_context->pb)
			avio_close(s_format_context->pb);
		av_free(s_format_context);
		s_format_context = nullptr;
	}

	if (s_sws_context)
	{
		sws_freeContext(s_sws_context);
		s_sws_context = nullptr;
	}
}

void AVIDump::DoState()
{
	s_last_frame = CoreTiming::GetTicks();
}
