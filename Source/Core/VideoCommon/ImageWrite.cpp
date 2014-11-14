// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <list>
#include <string>
#include <vector>

#include "png.h"
#include "Common/FileUtil.h"
#include "VideoCommon/ImageWrite.h"

bool SaveData(const std::string& filename, const char* data)
{
	std::ofstream f;
	OpenFStream(f, filename, std::ios::binary);
	f << data;

	return true;
}


/*
TextureToPng

Inputs:
data      : This is an array of RGBA with 8 bits per channel. 4 bytes for each pixel.
row_stride: Determines the amount of bytes per row of pixels.
*/
bool TextureToPng(u8* data, int row_stride, const std::string& filename, int width, int height, bool saveAlpha)
{
	bool success = false;

	if (!data)
		return false;

	char title[] = "Dolphin Screenshot";
	char title_key[] = "Title";
	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;

	// Open file for writing (binary mode)
	File::IOFile fp(filename, "wb");
	if (!fp.IsOpen())
	{
		PanicAlert("Screenshot failed: Could not open file %s %d\n", filename.c_str(), errno);
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (png_ptr == nullptr)
	{
		PanicAlert("Screenshot failed: Could not allocate write struct\n");
		goto finalise;

	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr)
	{
		PanicAlert("Screenshot failed: Could not allocate info struct\n");
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		PanicAlert("Screenshot failed: Error during png creation\n");
		goto finalise;
	}

	png_init_io(png_ptr, fp.GetHandle());

	// Write header (8 bit color depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
		8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = title_key;
	title_text.text = title;
	png_set_text(png_ptr, info_ptr, &title_text, 1);

	png_write_info(png_ptr, info_ptr);

	// Write image data
	for (auto y = 0; y < height; ++y)
	{
		u8* row_ptr = (u8*)data + y * row_stride;
		u8* ptr = row_ptr;
		for (auto x = 0; x < row_stride / 4; ++x)
		{
			if (!saveAlpha)
				ptr[3] = 0xff;
			ptr += 4;
		}
		png_write_row(png_ptr, row_ptr);
	}

	// End write
	png_write_end(png_ptr, nullptr);

	success = true;

finalise:
	if (info_ptr != nullptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != nullptr) png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);

	return success;
}
