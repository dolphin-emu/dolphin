// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif

#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h" //for TargetRefreshRate
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _WIN32

#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

#include "Core/ConfigManager.h" // for PAL60
#include "Core/CoreTiming.h"

static HWND s_emu_wnd;
static LONG s_byte_buffer;
static LONG s_frame_count;
static LONG s_total_bytes;
static PAVIFILE s_file;
static int s_width;
static int s_height;
static int s_file_count;
static u64 s_last_frame;
static PAVISTREAM s_stream;
static PAVISTREAM s_stream_compressed;
static int s_frame_rate;
static AVISTREAMINFO s_header;
static AVICOMPRESSOPTIONS s_options;
static AVICOMPRESSOPTIONS* s_array_options[1];
static BITMAPINFOHEADER s_bitmap;
// the CFR dump design doesn't let you dump until you know the NEXT timecode.
// so we have to save a frame and always be behind
static void* s_stored_frame = nullptr;
static u64 s_stored_frame_size = 0;
static bool s_start_dumping = false;

bool AVIDump::Start(HWND hWnd, int w, int h)
{
	s_emu_wnd = hWnd;
	s_file_count = 0;

	s_width = w;
	s_height = h;

	s_last_frame = CoreTiming::GetTicks();

	if (SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.E60"))
		s_frame_rate = 60; // always 60, for either pal60 or ntsc
	else
		s_frame_rate = VideoInterface::TargetRefreshRate; // 50 or 60, depending on region

	// clear CFR frame cache on start, not on file create (which is also segment switch)
	SetBitmapFormat();
	StoreFrame(nullptr);

	return CreateFile();
}

bool AVIDump::CreateFile()
{
	s_total_bytes = 0;
	s_frame_count = 0;

	std::string movie_file_name = StringFromFormat("%sframedump%d.avi", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), s_file_count);

	// Create path
	File::CreateFullPath(movie_file_name);

	// Ask to delete file
	if (File::Exists(movie_file_name))
	{
		if (AskYesNoT("Delete the existing file '%s'?", movie_file_name.c_str()))
			File::Delete(movie_file_name);
	}

	AVIFileInit();
	NOTICE_LOG(VIDEO, "Opening AVI file (%s) for dumping", movie_file_name.c_str());
	// TODO: Make this work with AVIFileOpenW without it throwing REGDB_E_CLASSNOTREG
	HRESULT hr = AVIFileOpenA(&s_file, movie_file_name.c_str(), OF_WRITE | OF_CREATE, nullptr);
	if (FAILED(hr))
	{
		if (hr == AVIERR_BADFORMAT) NOTICE_LOG(VIDEO, "The file couldn't be read, indicating a corrupt file or an unrecognized format.");
		if (hr == AVIERR_MEMORY)  NOTICE_LOG(VIDEO, "The file could not be opened because of insufficient memory.");
		if (hr == AVIERR_FILEREAD) NOTICE_LOG(VIDEO, "A disk error occurred while reading the file.");
		if (hr == AVIERR_FILEOPEN) NOTICE_LOG(VIDEO, "A disk error occurred while opening the file.");
		if (hr == REGDB_E_CLASSNOTREG) NOTICE_LOG(VIDEO, "AVI class not registered");
		Stop();
		return false;
	}

	SetBitmapFormat();
	NOTICE_LOG(VIDEO, "Setting video format...");
	if (!SetVideoFormat())
	{
		NOTICE_LOG(VIDEO, "Setting video format failed");
		Stop();
		return false;
	}

	if (!s_file_count)
	{
		if (!SetCompressionOptions())
		{
			NOTICE_LOG(VIDEO, "SetCompressionOptions failed");
			Stop();
			return false;
		}
	}

	if (FAILED(AVIMakeCompressedStream(&s_stream_compressed, s_stream, &s_options, nullptr)))
	{
		NOTICE_LOG(VIDEO, "AVIMakeCompressedStream failed");
		Stop();
		return false;
	}

	if (FAILED(AVIStreamSetFormat(s_stream_compressed, 0, &s_bitmap, s_bitmap.biSize)))
	{
		NOTICE_LOG(VIDEO, "AVIStreamSetFormat failed");
		Stop();
		return false;
	}

	return true;
}

void AVIDump::CloseFile()
{
	if (s_stream_compressed)
	{
		AVIStreamClose(s_stream_compressed);
		s_stream_compressed = nullptr;
	}

	if (s_stream)
	{
		AVIStreamClose(s_stream);
		s_stream = nullptr;
	}

	if (s_file)
	{
		AVIFileRelease(s_file);
		s_file = nullptr;
	}

	AVIFileExit();
}

void AVIDump::Stop()
{
	// store one copy of the last video frame, CFR case
	if (s_stream_compressed)
		AVIStreamWrite(s_stream_compressed, s_frame_count++, 1, GetFrame(), s_bitmap.biSizeImage, AVIIF_KEYFRAME, nullptr, &s_byte_buffer);
	s_start_dumping = false;
	CloseFile();
	s_file_count = 0;
	NOTICE_LOG(VIDEO, "Stop");
}

void AVIDump::StoreFrame(const void* data)
{
	if (s_bitmap.biSizeImage > s_stored_frame_size)
	{
		void* temp_stored_frame = realloc(s_stored_frame, s_bitmap.biSizeImage);
		if (temp_stored_frame)
		{
			s_stored_frame = temp_stored_frame;
		}
		else
		{
			free(s_stored_frame);
			PanicAlert("Something has gone seriously wrong.\n"
				"Stopping video recording.\n"
				"Your video will likely be broken.");
			Stop();
		}
		s_stored_frame_size = s_bitmap.biSizeImage;
		memset(s_stored_frame, 0, s_bitmap.biSizeImage);
	}
	if (s_stored_frame)
	{
		if (data)
			memcpy(s_stored_frame, data, s_bitmap.biSizeImage);
		else // pitch black frame
			memset(s_stored_frame, 0, s_bitmap.biSizeImage);
	}
}

void* AVIDump::GetFrame()
{
	return s_stored_frame;
}

void AVIDump::AddFrame(const u8* data, int w, int h)
{
	static bool shown_error = false;
	if ((w != s_bitmap.biWidth || h != s_bitmap.biHeight) && !shown_error)
	{
		PanicAlert("You have resized the window while dumping frames.\n"
			"Nothing sane can be done to handle this.\n"
			"Your video will likely be broken.");
		shown_error = true;

		s_bitmap.biWidth = w;
		s_bitmap.biHeight = h;
	}
	// no timecodes, instead dump each frame as many/few times as needed to keep sync
	u64 one_cfr = SystemTimers::GetTicksPerSecond() / VideoInterface::TargetRefreshRate;
	int nplay = 0;
	s64 delta;
	if (!s_start_dumping && s_last_frame <= SystemTimers::GetTicksPerSecond())
	{
		delta = CoreTiming::GetTicks();
		s_start_dumping = true;
	}
	else
	{
		delta = CoreTiming::GetTicks() - s_last_frame;
	}
	bool b_frame_dumped = false;
	// try really hard to place one copy of frame in stream (otherwise it's dropped)
	if (delta > (s64)one_cfr * 3 / 10) // place if 3/10th of a frame space
	{
		delta -= one_cfr;
		nplay++;
	}
	// try not nearly so hard to place additional copies of the frame
	while (delta > (s64)one_cfr * 8 / 10) // place if 8/10th of a frame space
	{
		delta -= one_cfr;
		nplay++;
	}
	while (nplay--)
	{
		if (!b_frame_dumped)
		{
			AVIStreamWrite(s_stream_compressed, s_frame_count++, 1, GetFrame(), s_bitmap.biSizeImage, AVIIF_KEYFRAME, nullptr, &s_byte_buffer);
			b_frame_dumped = true;
		}
		else
		{
			AVIStreamWrite(s_stream, s_frame_count++, 1, nullptr, 0, 0, nullptr, nullptr);
		}
		s_total_bytes += s_byte_buffer;
		// Close the recording if the file is larger than 2gb
		// VfW can't properly save files over 2gb in size, but can keep writing to them up to 4gb.
		if (s_total_bytes >= 2000000000)
		{
			CloseFile();
			s_file_count++;
			CreateFile();
		}
	}
	StoreFrame(data);
	s_last_frame = CoreTiming::GetTicks();
}

void AVIDump::SetBitmapFormat()
{
	memset(&s_bitmap, 0, sizeof(s_bitmap));
	s_bitmap.biSize = 0x28;
	s_bitmap.biPlanes = 1;
	s_bitmap.biBitCount = 24;
	s_bitmap.biWidth = s_width;
	s_bitmap.biHeight = s_height;
	s_bitmap.biSizeImage = 3 * s_width * s_height;
}

bool AVIDump::SetCompressionOptions()
{
	memset(&s_options, 0, sizeof(s_options));
	s_array_options[0] = &s_options;

	return (AVISaveOptions(s_emu_wnd, 0, 1, &s_stream, s_array_options) != 0);
}

bool AVIDump::SetVideoFormat()
{
	memset(&s_header, 0, sizeof(s_header));
	s_header.fccType = streamtypeVIDEO;
	s_header.dwScale = 1;
	s_header.dwRate = s_frame_rate;
	s_header.dwSuggestedBufferSize  = s_bitmap.biSizeImage;

	return SUCCEEDED(AVIFileCreateStream(s_file, &s_stream, &s_header));
}

#else

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>
}

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

	if (!(s_format_context->oformat = av_guess_format("avi", nullptr, nullptr)) ||
	    !(s_stream = avformat_new_stream(s_format_context, codec)))
	{
		return false;
	}

	s_stream->codec->codec_id = g_Config.bUseFFV1 ? AV_CODEC_ID_FFV1
	                                              : s_format_context->oformat->video_codec;
	s_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	s_stream->codec->bit_rate = 400000;
	s_stream->codec->width = s_width;
	s_stream->codec->height = s_height;
	s_stream->codec->time_base = (AVRational){1, static_cast<int>(VideoInterface::TargetRefreshRate)};
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
	if (!s_start_dumping && s_last_frame <= SystemTimers::GetTicksPerSecond())
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
		if (pkt.pts != AV_NOPTS_VALUE)
		{
			pkt.pts = av_rescale_q(pkt.pts,
			                       s_stream->codec->time_base, s_stream->time_base);
		}
		if (pkt.dts != AV_NOPTS_VALUE)
		{
			pkt.dts = av_rescale_q(pkt.dts,
			                       s_stream->codec->time_base, s_stream->time_base);
		}
		if (s_stream->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
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

#endif

void AVIDump::DoState()
{
	s_last_frame = CoreTiming::GetTicks();
}
