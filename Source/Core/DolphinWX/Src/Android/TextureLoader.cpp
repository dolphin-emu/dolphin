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

#include "GLInterface.h"
#include <png.h>

GLuint LoadPNG(const char *filename)
{
	FILE         *infile;         /* PNG file pointer */
	png_structp   png_ptr;        /* internally used by libpng */
	png_infop     info_ptr;       /* user requested transforms */

	char         *image_data;      /* raw png image data */
	char         sig[8];           /* PNG signature array */
	/*char         **row_pointers;   */

	int           bit_depth;
	int           color_type;

	png_uint_32 width;            /* PNG image width in pixels */
	png_uint_32 height;           /* PNG image height in pixels */
	unsigned int rowbytes;         /* raw bytes at row n in image */

	image_data = NULL;
	unsigned int i;
	png_bytepp row_pointers = NULL;

	/* Open the file. */
	infile = fopen(filename, "rb");
	if (!infile) 
		return 0;

	/*
	*              13.3 readpng_init()
	*/

	/* Check for the 8-byte signature */
	fread(sig, 1, 8, infile);

	if (!png_check_sig((unsigned char *) sig, 8)) {
		fclose(infile);
		return 0;
	}

	/* 
	* Set up the PNG structs 
	*/
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(infile);
		return 4;    /* out of memory */
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		fclose(infile);
		return 4;    /* out of memory */
	}


	/*
	* block to handle libpng errors, 
	* then check whether the PNG file had a bKGD chunk
	*/
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(infile);
		return 0;
	}

	/* 
	* takes our file stream pointer (infile) and 
	* stores it in the png_ptr struct for later use.
	*/
	/* png_ptr->io_ptr = (png_voidp)infile;*/
	png_init_io(png_ptr, infile);

	/*
	* lets libpng know that we already checked the 8 
	* signature bytes, so it should not expect to find 
	* them at the current file pointer location
	*/
	png_set_sig_bytes(png_ptr, 8);

	/* Read the image */

	/*
	* reads and processes not only the PNG file's IHDR chunk 
	* but also any other chunks up to the first IDAT 
	* (i.e., everything before the image data).
	*/

	/* read all the info up to the image data  */
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, 
	&color_type, NULL, NULL, NULL);

	/* Set up some transforms. */
	if (bit_depth > 8) 
		png_set_strip_16(png_ptr);
	
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) 
		png_set_gray_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE) 
		png_set_palette_to_rgb(png_ptr);

	/* Update the png info struct.*/
	png_read_update_info(png_ptr, info_ptr);

	/* Rowsize in bytes. */
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);


	/* Allocate the image_data buffer. */
	if ((image_data = (char *) malloc(rowbytes * height))==NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 4;
	}

	if ((row_pointers = (png_bytepp)malloc(height*sizeof(png_bytep))) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(image_data);
		image_data = NULL;
		return 4;
	}


	/* set the individual row_pointers to point at the correct offsets */

	for (i = 0;  i < height;  ++i)
		row_pointers[i] = (png_byte*)(image_data + i*rowbytes);


	/* now we can go ahead and just read the whole image */
	png_read_image(png_ptr, row_pointers);

	/* and we're done!  (png_read_end() can be omitted if no processing of
	* post-IDAT text/time/etc. is desired) */

	/* Clean up. */
	free(row_pointers);

	/* Clean up. */
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(infile);

	GLuint Texture = 0;
	glGenTextures(1, &Texture);

	/* create a new texture object
	* and bind it to texname (unsigned integer > 0)
	*/
	glBindTexture(GL_TEXTURE_2D, Texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
				GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	return Texture;
}
