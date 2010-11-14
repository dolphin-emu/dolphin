// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "AVIDump.h"

#ifdef _WIN32

#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

#include "FileUtil.h"
#include "CommonPaths.h"
#include "Log.h"

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
	sprintf(movie_file_name, "%sframedump%d.avi", File::GetUserPath(D_DUMPFRAMES_IDX), m_fileCount);
	// Create path
	File::CreateFullPath(movie_file_name);

	// Ask to delete file
	if (File::Exists(movie_file_name))
	{
		if (AskYesNo("Delete the existing file '%s'?", movie_file_name))
			File::Delete(movie_file_name);
	}

	AVIFileInit();
	NOTICE_LOG(VIDEO, "Opening AVI file (%s) for dumping", movie_file_name);
	// TODO: Make this work with AVIFileOpenW without it throwing REGDB_E_CLASSNOTREG
	HRESULT hr = AVIFileOpenA(&m_file, movie_file_name, OF_WRITE | OF_CREATE, NULL);
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

	if (!m_fileCount) {
		if (!SetCompressionOptions()) {
			NOTICE_LOG(VIDEO, "SetCompressionOptions failed");
			Stop();
			return false;
		}
	}

	if (FAILED(AVIMakeCompressedStream(&m_streamCompressed, m_stream, &m_options, NULL)))
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
		m_streamCompressed = NULL;
	}

	if (m_stream)
	{
		AVIStreamClose(m_stream);
		m_stream = NULL;
	}

	if (m_file)
	{
		AVIFileRelease(m_file);
		m_file = NULL;
	}

	AVIFileExit();
}

void AVIDump::Stop()
{
	CloseFile();
	m_fileCount = 0;
	NOTICE_LOG(VIDEO, "Stop");
}

void AVIDump::AddFrame(char *data)
{
	AVIStreamWrite(m_streamCompressed, ++m_frameCount, 1, (LPVOID) data, m_bitmap.biSizeImage, AVIIF_KEYFRAME, NULL, &m_byteBuffer);
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
	// TODO: Decect FPS using NTSC/PAL
	m_header.dwRate = 60;
	m_header.dwSuggestedBufferSize  = m_bitmap.biSizeImage;

	return SUCCEEDED(AVIFileCreateStream(m_file, &m_stream, &m_header));
}

#else

#include "FileUtil.h"
#include "StringUtil.h"
#include "Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

FILE* f_pFrameDump = NULL;
AVCodec *s_Codec = NULL;
AVCodecContext *s_CodecContext = NULL;
AVFrame *s_BGRFrame = NULL, *s_YUVFrame = NULL;
uint8_t *s_YUVBuffer = NULL;
uint8_t *s_OutBuffer = NULL;
struct SwsContext *s_SwsContext = NULL;
int s_width;
int s_height;
int s_size;

static void InitAVCodec()
{
	static bool first_run = true;
	if (first_run)
	{
		avcodec_init();
		avcodec_register_all();
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
	s_Codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
	if (!s_Codec)
		return false;

	s_CodecContext = avcodec_alloc_context();

	s_CodecContext->bit_rate = 400000;
	s_CodecContext->width = s_width;
	s_CodecContext->height = s_height;
	s_CodecContext->time_base = (AVRational){1, 60};
	s_CodecContext->gop_size = 10;
	s_CodecContext->max_b_frames = 1;
	s_CodecContext->pix_fmt = PIX_FMT_YUV420P;

	NOTICE_LOG(VIDEO, "Opening MPG file framedump.mpg for dumping");
	if ((avcodec_open(s_CodecContext, s_Codec) < 0) ||
			!(f_pFrameDump = fopen(StringFromFormat("%sframedump.mpg",
						File::GetUserPath(D_DUMPFRAMES_IDX)).c_str(), "wb")) ||
			!(s_SwsContext = sws_getContext(s_width, s_height, PIX_FMT_BGR24, s_width, s_height,
					PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL)))
	{
		WARN_LOG(VIDEO, "Unable to initialize mpg dump file");
		CloseFile();
		return false;
	}

	s_BGRFrame = avcodec_alloc_frame();
	s_YUVFrame = avcodec_alloc_frame();

	s_size = avpicture_get_size(PIX_FMT_YUV420P, s_width, s_height);

	s_YUVBuffer = new uint8_t[s_size];
	avpicture_fill((AVPicture *)s_YUVFrame, s_YUVBuffer, PIX_FMT_YUV420P, s_width, s_height);

	s_OutBuffer = new uint8_t[s_size];

	return true;
}

void AVIDump::AddFrame(uint8_t *data)
{
	avpicture_fill((AVPicture *)s_BGRFrame, data, PIX_FMT_BGR24, s_width, s_height);

	// Convert image from BGR24 to YUV420P
	sws_scale(s_SwsContext, s_BGRFrame->data, s_BGRFrame->linesize, 0,
			s_height, s_YUVFrame->data, s_YUVFrame->linesize);

	// Encode and write the image
	int s_iOutSize = avcodec_encode_video(s_CodecContext, s_OutBuffer, s_size, s_YUVFrame);
	fwrite(s_OutBuffer, 1, s_iOutSize, f_pFrameDump);
	fflush(f_pFrameDump);

	// Encode and write delayed frames
	do
	{
		s_iOutSize = avcodec_encode_video(s_CodecContext, s_OutBuffer, s_size, NULL);
		fwrite(s_OutBuffer, 1, s_iOutSize, f_pFrameDump);
		fflush(f_pFrameDump);
	} while (s_iOutSize);
}

void AVIDump::Stop()
{
	// Write MPG footer
	s_OutBuffer[0] = 0x00;
	s_OutBuffer[1] = 0x00;
	s_OutBuffer[2] = 0x01;
	s_OutBuffer[3] = 0xb7;
	fwrite(s_OutBuffer, 1, 4, f_pFrameDump);
	fflush(f_pFrameDump);

	CloseFile();
	NOTICE_LOG(VIDEO, "Stopping frame dump");
}

void AVIDump::CloseFile()
{
	if (f_pFrameDump)
		fclose(f_pFrameDump);
	f_pFrameDump = NULL;

	if (s_YUVBuffer)
		delete[] s_YUVBuffer;
	s_YUVBuffer = NULL;

	if (s_OutBuffer)
		delete[] s_OutBuffer;
	s_OutBuffer = NULL;

	if (s_CodecContext)
	{
		avcodec_close(s_CodecContext);
		av_free(s_CodecContext);
	}
	s_CodecContext = NULL;

	if (s_BGRFrame)
		av_free(s_BGRFrame);
	s_BGRFrame = NULL;

	if (s_YUVFrame)
		av_free(s_YUVFrame);
	s_YUVFrame = NULL;

	if (s_SwsContext)
		sws_freeContext(s_SwsContext);
	s_SwsContext = NULL;
}

#endif
