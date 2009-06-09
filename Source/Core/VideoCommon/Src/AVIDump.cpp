// Copyright (C) 2003-2009 Dolphin Project.

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
#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

#include "FileUtil.h"
#include "CommonPaths.h"
#include "Log.h"

static HWND m_emuWnd;
static int m_width;
static int m_height;
static LONG m_byteBuffer;
static LONG m_frameCount;
static LONG m_totalBytes;
static PAVIFILE m_file;
static int m_fileCount;
static PAVISTREAM m_stream;
static PAVISTREAM m_streamCompressed;
static AVISTREAMINFO m_header;
static AVICOMPRESSOPTIONS m_options;
static AVICOMPRESSOPTIONS *m_arrayOptions[1];
static BITMAPINFOHEADER m_bitmap;

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
	sprintf(movie_file_name, "%s/framedump%d.avi", FULL_FRAMES_DIR, m_fileCount);
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
	if (FAILED(hr)) {
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
	if (!SetVideoFormat()) {
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
	if (FAILED(AVIMakeCompressedStream(&m_streamCompressed, m_stream, &m_options, NULL))) {
		NOTICE_LOG(VIDEO, "AVIMakeCompressedStream failed");
		Stop();
		return false;
	}
	if (FAILED(AVIStreamSetFormat(m_streamCompressed, 0, &m_bitmap, m_bitmap.biSize))) {
		NOTICE_LOG(VIDEO, "AVIStreamSetFormat failed");
		Stop();
		return false;
	}

	return true;
}

void AVIDump::CloseFile()
{
	if (m_streamCompressed) {
		AVIStreamClose(m_streamCompressed);
		m_streamCompressed = NULL;
	}
	if (m_stream) {
		AVIStreamClose(m_stream);
		m_stream = NULL;
	}
	if (m_file) {
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
	if (m_totalBytes >= 2000000000) {
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
