// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <list>
#include <vector>

#include "png.h"
#include "ImageWrite.h"
#include "FileUtil.h"

#pragma pack(push, 1)

struct TGA_HEADER
{
	u8  identsize;          // size of ID field that follows 18 u8 header (0 usually)
	u8  colourmaptype;      // type of colour map 0=none, 1=has palette
	u8  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	s16 colourmapstart;     // first colour map entry in palette
	s16 colourmaplength;    // number of colours in palette
	u8  colourmapbits;      // number of bits per palette entry 15,16,24,32

	s16 xstart;             // image x origin
	s16 ystart;             // image y origin
	s16 width;              // image width in pixels
	s16 height;             // image height in pixels
	u8  bits;               // image bits per pixel 8,16,24,32
	u8  descriptor;         // image descriptor bits (vh flip bits)

	// pixel data follows header
};

#pragma pack(pop)

bool SaveTGA(const char* filename, int width, int height, void* pdata)
{
	TGA_HEADER hdr;
	File::IOFile f(filename, "wb");
	if (!f)
		return false;

	_assert_(sizeof(TGA_HEADER) == 18 && sizeof(hdr) == 18);

	memset(&hdr, 0, sizeof(hdr));
	hdr.imagetype = 2;
	hdr.bits = 32;
	hdr.width = width;
	hdr.height = height;
	hdr.descriptor |= 8|(1<<5); // 8bit alpha, flip vertical

	f.WriteArray(&hdr, 1);
	f.WriteBytes(pdata, width * height * 4);

	return true;
}

bool SaveData(const char* filename, const char* data)
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
data is a newly allocated memory and must have delete[] run on it before returning.

row_stride: Determines the amount of bytes per row of pixels.
*/
bool TextureToPng(u8* data, int row_stride, const char* filename, int width, int height, bool saveAlpha)
{
	bool success = false;

	if (!data)
		return false;

	// Open file for writing (binary mode)
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		PanicAlert("Screenshot failed: Could not open file %s\n", filename);
		goto finalise;
	}

	// Initialize write structure
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		PanicAlert("Screenshot failed: Could not allocate write struct\n");
		goto finalise;

	}

	// Initialize info structure
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		PanicAlert("Screenshot failed: Could not allocate info struct\n");
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		PanicAlert("Screenshot failed: Error during png creation\n");
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit colour depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
		8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	char title[] = "Dolphin Screenshot";
	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = "Title";
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
	png_write_end(png_ptr, NULL);

	success = true;

finalise:

	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	// Our duty to delete the inputted data.
	delete[] data;

	return success;
}
