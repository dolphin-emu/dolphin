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

#include "Core/HW/VideoInterface.h" //for TargetRefreshRate
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _WIN32

#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

HWND m_emuWnd;
LONG m_byteBuffer;
LONG m_frameCount;
LONG m_totalBytes;
PAVIFILE m_file;
int m_width;
int m_height;
int m_fileCount;
PAVISTREAM m_stream;
PAVISTREAM m_streamCompressed;
AVISTREAMINFO m_header;
AVICOMPRESSOPTIONS m_options;
AVICOMPRESSOPTIONS *m_arrayOptions[1];
BITMAPINFOHEADER m_bitmap;

bool AVIDump::Start(HWND hWnd, int w, int h)
{
	m_emuWnd = hWnd;
	m_fileCount = 0;

	m_width = w;
	m_height = h;

	return CreateFile();
}

bool AVIDump::CreateFile()
{
	m_totalBytes = 0;
	m_frameCount = 0;

	std::string movie_file_name = StringFromFormat("%sframedump%d.avi", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), m_fileCount);

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
	HRESULT hr = AVIFileOpenA(&m_file, movie_file_name.c_str(), OF_WRITE | OF_CREATE, nullptr);
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

	if (!m_fileCount)
	{
		if (!SetCompressionOptions())
		{
			NOTICE_LOG(VIDEO, "SetCompressionOptions failed");
			Stop();
			return false;
		}
	}

	if (FAILED(AVIMakeCompressedStream(&m_streamCompressed, m_stream, &m_options, nullptr)))
	{
		NOTICE_LOG(VIDEO, "AVIMakeCompressedStream failed");
		Stop();
		return false;
	}

	if (FAILED(AVIStreamSetFormat(m_streamCompressed, 0, &m_bitmap, m_bitmap.biSize)))
	{
		NOTICE_LOG(VIDEO, "AVIStreamSetFormat failed");
		Stop();
		return false;
	}

	return true;
}

void AVIDump::CloseFile()
{
	if (m_streamCompressed)
	{
		AVIStreamClose(m_streamCompressed);
		m_streamCompressed = nullptr;
	}

	if (m_stream)
	{
		AVIStreamClose(m_stream);
		m_stream = nullptr;
	}

	if (m_file)
	{
		AVIFileRelease(m_file);
		m_file = nullptr;
	}

	AVIFileExit();
}

void AVIDump::Stop()
{
	CloseFile();
	m_fileCount = 0;
	NOTICE_LOG(VIDEO, "Stop");
}

void AVIDump::AddFrame(const u8* data, int w, int h)
{
	static bool shown_error = false;
	if ((w != m_bitmap.biWidth || h != m_bitmap.biHeight) && !shown_error)
	{
		PanicAlert("You have resized the window while dumping frames.\n"
			"Nothing sane can be done to handle this.\n"
			"Your video will likely be broken.");
		shown_error = true;

		m_bitmap.biWidth = w;
		m_bitmap.biHeight = h;
	}

	AVIStreamWrite(m_streamCompressed, ++m_frameCount, 1, const_cast<u8*>(data), m_bitmap.biSizeImage, AVIIF_KEYFRAME, nullptr, &m_byteBuffer);
	m_totalBytes += m_byteBuffer;
	// Close the recording if the file is more than 2gb
	// VfW can't properly save files over 2gb in size, but can keep writing to them up to 4gb.
	if (m_totalBytes >= 2000000000)
	{
		CloseFile();
		m_fileCount++;
		CreateFile();
	}
}

void AVIDump::SetBitmapFormat()
{
	memset(&m_bitmap, 0, sizeof(m_bitmap));
	m_bitmap.biSize = 0x28;
	m_bitmap.biPlanes = 1;
	m_bitmap.biBitCount = 24;
	m_bitmap.biWidth = m_width;
	m_bitmap.biHeight = m_height;
	m_bitmap.biSizeImage = 3 * m_width * m_height;
}

bool AVIDump::SetCompressionOptions()
{
	memset(&m_options, 0, sizeof(m_options));
	m_arrayOptions[0] = &m_options;

	return (AVISaveOptions(m_emuWnd, 0, 1, &m_stream, m_arrayOptions) != 0);
}

bool AVIDump::SetVideoFormat()
{
	memset(&m_header, 0, sizeof(m_header));
	m_header.fccType = streamtypeVIDEO;
	m_header.dwScale = 1;
	m_header.dwRate = VideoInterface::TargetRefreshRate;
	m_header.dwSuggestedBufferSize  = m_bitmap.biSizeImage;

	return SUCCEEDED(AVIFileCreateStream(m_file, &m_stream, &m_header));
}

#else

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
	s_stream->codec->time_base = (AVRational){1, (int)VideoInterface::TargetRefreshRate};
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
	if (s_stream->codec->coded_frame->pts != AV_NOPTS_VALUE)
	{
		pkt->pts = av_rescale_q(s_stream->codec->coded_frame->pts,
		                        s_stream->codec->time_base, s_stream->time_base);
	}
	if (s_stream->codec->coded_frame->key_frame)
		pkt->flags |= AV_PKT_FLAG_KEY;
	pkt->stream_index = s_stream->index;
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
	int got_packet;
	int error = avcodec_encode_video2(s_stream->codec, &pkt, s_scaled_frame, &got_packet);
	while (!error && got_packet)
	{
		// Write the compressed frame in the media file.
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
