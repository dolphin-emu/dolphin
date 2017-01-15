// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <list>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/ImageWrite.h"
#include "png.h"

bool SaveData(const std::string& filename, const std::string& data)
{
  std::ofstream f;
  File::OpenFStream(f, filename, std::ios::binary);
  f << data;

  return true;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4611)
#endif

/*
TextureToPng

Inputs:
data      : This is an array of RGBA with 8 bits per channel. 4 bytes for each pixel.
row_stride: Determines the amount of bytes per row of pixels.
*/
bool TextureToPng(const u8* data, int row_stride, const std::string& filename, int width,
                  int height, bool saveAlpha)
{
  if (!data)
    return false;

  bool success = false;
  char title[] = "Dolphin Screenshot";
  char title_key[] = "Title";
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  std::vector<u8> buffer;

  // Open file for writing (binary mode)
  File::IOFile fp(filename, "wb");
  if (!fp.IsOpen())
  {
    PanicAlertT("Screenshot failed: Could not open file \"%s\" (error %d)", filename.c_str(),
                errno);
    goto finalise;
  }

  // Initialize write structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (png_ptr == nullptr)
  {
    PanicAlert("Screenshot failed: Could not allocate write struct");
    goto finalise;
  }

  // Initialize info structure
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == nullptr)
  {
    PanicAlert("Screenshot failed: Could not allocate info struct");
    goto finalise;
  }

  // Classical libpng error handling uses longjmp to do C-style unwind.
  // Modern libpng does support a user callback, but it's required to operate
  // in the same way (just gives a chance to do stuff before the longjmp).
  // Instead of futzing with it, we use gotos specifically so the compiler
  // will still generate proper destructor calls for us (hopefully).
  // We also do not use any local variables outside the region longjmp may
  // have been called from if they were modified inside that region (they
  // would need to be volatile).
  if (setjmp(png_jmpbuf(png_ptr)))
  {
    PanicAlert("Screenshot failed: Error during PNG creation");
    goto finalise;
  }

  // Begin region which may call longjmp

  png_init_io(png_ptr, fp.GetHandle());

  // Write header (8 bit color depth)
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_text title_text;
  title_text.compression = PNG_TEXT_COMPRESSION_NONE;
  title_text.key = title_key;
  title_text.text = title;
  png_set_text(png_ptr, info_ptr, &title_text, 1);

  png_write_info(png_ptr, info_ptr);

  if (!saveAlpha)
    buffer.resize(width * 4);

  // Write image data
  for (auto y = 0; y < height; ++y)
  {
    const u8* row_ptr = data + y * row_stride;
    if (!saveAlpha)
    {
      for (int x = 0; x < width; x++)
      {
        for (int i = 0; i < 3; i++)
          buffer[4 * x + i] = row_ptr[4 * x + i];
        buffer[4 * x + 3] = 0xff;
      }
      row_ptr = buffer.data();
    }

    // The old API uses u8* instead of const u8*. It doesn't write
    // to this pointer, but to fit the API, we have to drop the const qualifier.
    png_write_row(png_ptr, const_cast<u8*>(row_ptr));
  }

  // End write
  png_write_end(png_ptr, nullptr);

  // End region which may call longjmp

  success = true;

finalise:
  if (info_ptr != nullptr)
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  if (png_ptr != nullptr)
    png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);

  return success;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
