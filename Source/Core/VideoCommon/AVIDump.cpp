// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif

#include "Core/HW/VideoInterface.h" //for TargetRefreshRate
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _WIN32

#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Log.h"

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
	char movie_file_name[255];
	sprintf(movie_file_name, "%sframedump%d.avi", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), m_fileCount);
	// Create path
	File::CreateFullPath(movie_file_name);

	// Ask to delete file
	if (File::Exists(movie_file_name))
	{
		if (AskYesNoT("Delete the existing file '%s'?", movie_file_name))
			File::Delete(movie_file_name);
	}

	AVIFileInit();
	NOTICE_LOG(VIDEO, "Opening AVI file (%s) for dumping", movie_file_name);
	// TODO: Make this work with AVIFileOpenW without it throwing REGDB_E_CLASSNOTREG
	HRESULT hr = AVIFileOpenA(&m_file, movie_file_name, OF_WRITE | OF_CREATE, nullptr);
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

#include "Common/FileUtil.h"
#include "Common/Log.h"
#include "Common/StringUtil.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>
}

AVFormatContext *s_FormatContext = nullptr;
AVStream *s_Stream = nullptr;
AVFrame *s_BGRFrame = nullptr, *s_YUVFrame = nullptr;
uint8_t *s_YUVBuffer = nullptr;
uint8_t *s_OutBuffer = nullptr;
int s_width;
int s_height;
int s_size;

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
	return CreateFile();
}

bool AVIDump::CreateFile()
{
	AVCodec *codec = nullptr;

	s_FormatContext = avformat_alloc_context();
	snprintf(s_FormatContext->filename, sizeof(s_FormatContext->filename), "%s",
			(File::GetUserPath(D_DUMPFRAMES_IDX) + "framedump0.avi").c_str());
	File::CreateFullPath(s_FormatContext->filename);

	if (!(s_FormatContext->oformat = av_guess_format("avi", nullptr, nullptr)) ||
			!(s_Stream = avformat_new_stream(s_FormatContext, codec)))
	{
		CloseFile();
		return false;
	}

	s_Stream->codec->codec_id =
		g_Config.bUseFFV1 ?  CODEC_ID_FFV1 : s_FormatContext->oformat->video_codec;
	s_Stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	s_Stream->codec->bit_rate = 400000;
	s_Stream->codec->width = s_width;
	s_Stream->codec->height = s_height;
	s_Stream->codec->time_base = (AVRational){1, static_cast<int>(VideoInterface::TargetRefreshRate)};
	s_Stream->codec->gop_size = 12;
	s_Stream->codec->pix_fmt = g_Config.bUseFFV1 ? PIX_FMT_BGRA : PIX_FMT_YUV420P;

	if (!(codec = avcodec_find_encoder(s_Stream->codec->codec_id)) ||
			(avcodec_open2(s_Stream->codec, codec, nullptr) < 0))
	{
		CloseFile();
		return false;
	}

	s_BGRFrame = avcodec_alloc_frame();
	s_YUVFrame = avcodec_alloc_frame();

	s_size = avpicture_get_size(s_Stream->codec->pix_fmt, s_width, s_height);

	s_YUVBuffer = new uint8_t[s_size];
	avpicture_fill((AVPicture *)s_YUVFrame, s_YUVBuffer, s_Stream->codec->pix_fmt, s_width, s_height);

	s_OutBuffer = new uint8_t[s_size];

	NOTICE_LOG(VIDEO, "Opening file %s for dumping", s_FormatContext->filename);
	if (avio_open(&s_FormatContext->pb, s_FormatContext->filename, AVIO_FLAG_WRITE) < 0)
	{
		WARN_LOG(VIDEO, "Could not open %s", s_FormatContext->filename);
		CloseFile();
		return false;
	}

	avformat_write_header(s_FormatContext, nullptr);

	return true;
}

void AVIDump::AddFrame(const u8* data, int width, int height)
{
	avpicture_fill((AVPicture *)s_BGRFrame, const_cast<u8*>(data), PIX_FMT_BGR24, width, height);

	// Convert image from BGR24 to desired pixel format, and scale to initial
	// width and height
	struct SwsContext *s_SwsContext;
	if ((s_SwsContext = sws_getContext(width, height, PIX_FMT_BGR24, s_width, s_height,
					s_Stream->codec->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr)))
	{
		sws_scale(s_SwsContext, s_BGRFrame->data, s_BGRFrame->linesize, 0,
				height, s_YUVFrame->data, s_YUVFrame->linesize);
		sws_freeContext(s_SwsContext);
	}

	// Encode and write the image
	int outsize = avcodec_encode_video(s_Stream->codec, s_OutBuffer, s_size, s_YUVFrame);
	while (outsize > 0)
	{
		AVPacket pkt;
		av_init_packet(&pkt);

		if (s_Stream->codec->coded_frame->pts != (unsigned int)AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q(s_Stream->codec->coded_frame->pts,
					s_Stream->codec->time_base, s_Stream->time_base);
		if (s_Stream->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = s_Stream->index;
		pkt.data = s_OutBuffer;
		pkt.size = outsize;

		// Write the compressed frame in the media file
		av_interleaved_write_frame(s_FormatContext, &pkt);

		// Encode delayed frames
		outsize = avcodec_encode_video(s_Stream->codec, s_OutBuffer, s_size, nullptr);
	}
}

void AVIDump::Stop()
{
	av_write_trailer(s_FormatContext);
	CloseFile();
	NOTICE_LOG(VIDEO, "Stopping frame dump");
}

void AVIDump::CloseFile()
{
	if (s_Stream)
	{
		if (s_Stream->codec)
			avcodec_close(s_Stream->codec);
		av_free(s_Stream);
		s_Stream = nullptr;
	}

	if (s_YUVBuffer)
		delete[] s_YUVBuffer;
	s_YUVBuffer = nullptr;

	if (s_OutBuffer)
		delete[] s_OutBuffer;
	s_OutBuffer = nullptr;

	if (s_BGRFrame)
		av_free(s_BGRFrame);
	s_BGRFrame = nullptr;

	if (s_YUVFrame)
		av_free(s_YUVFrame);
	s_YUVFrame = nullptr;

	if (s_FormatContext)
	{
		if (s_FormatContext->pb)
			avio_close(s_FormatContext->pb);
		av_free(s_FormatContext);
		s_FormatContext = nullptr;
	}
}

#endif
