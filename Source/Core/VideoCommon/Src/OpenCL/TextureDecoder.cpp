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

#include "TextureDecoder.h"
#include <CL/cl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

    size_t global;                      // global domain size for our calculation
    size_t local;                       // local domain size for our calculation

    cl_device_id device_id;             // compute device id 
    cl_context context;                 // compute context
    cl_command_queue commands;          // compute command queue
    
    
struct sDecoders
{
    cl_program program;                 // compute program
    cl_kernel kernel;                   // compute kernel
    const char **cKernel;
};
const char *Kernel = "\n" \
"__kernel void Decode( __local unsigned char *dst, __local const unsigned char *src, \n" \
"                       int width, int height) \n" \
"   int id = get_global_id(0); \n" \
"			for (int xy = 0; xy < height*width; xy += 4)"
"					for (int iy = 0; iy < 4; iy++, src += 8)"
"					{"
"						u16 *ptr = (u16 *)dst + ((xy / width) + iy) * width + (xy % width);"
"						u16 *s = (u16 *)src;"
"						for(int j = 0; j < 4; j++)"
"							*ptr++ = Common::swap16(*s++);"
"					}" \

sDecoders Decoders[] = { {NULL, NULL, &Kernel}, 
};
bool Inited = false;

// TODO: Deinit (clRelease...)

bool Init_OpenCL()
{
    int err;                            // error code returned from api calls
            
    // Connect to a compute device
    //
    int gpu = 1;
    err = clGetDeviceIDs(NULL, gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to create a device group!\n");
        return EXIT_FAILURE;
    }
  
    // Create a compute context 
    //
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!context)
    {
        printf("Error: Failed to create a compute context!\n");
        return EXIT_FAILURE;
    }

    // Create a command commands
    //
    commands = clCreateCommandQueue(context, device_id, 0, &err);
    if (!commands)
    {
        printf("Error: Failed to create a command commands!\n");
        return EXIT_FAILURE;
    }

    // Create the compute program from the source buffer
    //
    /*
    program = clCreateProgramWithSource(context, 1, (const char **) & KernelSource, NULL, &err);
    if (!program)
    {
        printf("Error: Failed to create compute program!\n");
        return EXIT_FAILURE;
    }

    // Build the program executable
    //
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];

        printf("Error: Failed to build program executable!\n");
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("%s\n", buffer);
        exit(1);
    }

    // Create the compute kernel in the program we wish to run
    //
    kernel = clCreateKernel(program, "Decoder", &err);
    if (!kernel || err != CL_SUCCESS)
    {
        printf("Error: Failed to create compute kernel!\n");
        exit(1);
    }  */
}

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
    if(!Inited)
    {
        // Not yet inited, let's init now
        // Need to make a init function later
        if(!Init_OpenCL())
            PanicAlert("OpenCL could not initialize successfully");
    }

	// clEnqueueNDRangeKernel


    /*switch (texformat)
    {
    case GX_TF_C4:
		if (tlutfmt == 2)
        {
			// Special decoding is required for TLUT format 5A3
            for (int y = 0; y < height; y += 8)
                for (int x = 0; x < width; x += 8)
                    for (int iy = 0; iy < 8; iy++, src += 4)
                        decodebytesC4_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src, tlutaddr);
        }
		else
		{
            for (int y = 0; y < height; y += 8)
                for (int x = 0; x < width; x += 8)
                    for (int iy = 0; iy < 8; iy++, src += 4)
                        decodebytesC4_To_Raw16((u16*)dst + (y + iy) * width + x, src, tlutaddr);
		}
        return GetPCFormatFromTLUTFormat(tlutfmt);
    case GX_TF_I4:
		{
			for (int y = 0; y < height; y += 8)
				for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 8; iy++, src += 4)
						for (int ix = 0; ix < 4; ix++)
						{
							int val = src[ix];
							dst[(y + iy) * width + x + ix * 2] = Convert4To8(val >> 4);
							dst[(y + iy) * width + x + ix * 2 + 1] = Convert4To8(val & 0xF);
						}
        }
       return PC_TEX_FMT_I4_AS_I8;
	case GX_TF_I8:  // speed critical
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 4; iy++, src += 8)
						memcpy(dst + (y + iy)*width+x, src, 8);
		}
		return PC_TEX_FMT_I8;
    case GX_TF_C8:
		if (tlutfmt == 2)
        {
			// Special decoding is required for TLUT format 5A3
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 8)
                    for (int iy = 0; iy < 4; iy++, src += 8)
                        decodebytesC8_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src, tlutaddr);
        }
		else
		{
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 8)
                    for (int iy = 0; iy < 4; iy++, src += 8)
                        decodebytesC8_To_Raw16((u16*)dst + (y + iy) * width + x, src, tlutaddr);
		}
        return GetPCFormatFromTLUTFormat(tlutfmt);
    case GX_TF_IA4:
        {
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 4; iy++, src += 8)
                        decodebytesIA4((u16*)dst + (y + iy) * width + x, src);
        }
		return PC_TEX_FMT_IA4_AS_IA8;
    case GX_TF_IA8:
        {
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
					for (int iy = 0; iy < 4; iy++, src += 8)
					{
						u16 *ptr = (u16 *)dst + (y + iy) * width + x;
						u16 *s = (u16 *)src;
						for(int j = 0; j < 4; j++)
							*ptr++ = Common::swap16(*s++);
					}

        }
		return PC_TEX_FMT_IA8;
    case GX_TF_C14X2: 
		if (tlutfmt == 2)
        {
			// Special decoding is required for TLUT format 5A3
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 4)
                    for (int iy = 0; iy < 4; iy++, src += 8)
                        decodebytesC14X2_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, (u16*)src, tlutaddr);
        }
		else
		{
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 4)
                    for (int iy = 0; iy < 4; iy++, src += 8)
                        decodebytesC14X2_To_Raw16((u16*)dst + (y + iy) * width + x, (u16*)src, tlutaddr);
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
    case GX_TF_RGB565:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
					for (int iy = 0; iy < 4; iy++, src += 8)
					{
						u16 *ptr = (u16 *)dst + (y + iy) * width + x;
						u16 *s = (u16 *)src;
						for(int j = 0; j < 4; j++)
							*ptr++ = Common::swap16(*s++);
					}
		}
		return PC_TEX_FMT_RGB565;
    case GX_TF_RGB5A3:
        {
            for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 4)
                    for (int iy = 0; iy < 4; iy++, src += 8)
                        //decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)src, 4);
                        decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)src);
        }
        return PC_TEX_FMT_BGRA32;
    case GX_TF_RGBA8:  // speed critical
        {
			for (int y = 0; y < height; y += 4)
                for (int x = 0; x < width; x += 4)
                {
					for (int iy = 0; iy < 4; iy++)
                        decodebytesARGB8_4((u32*)dst + (y+iy)*width + x, (u16*)src + 4 * iy, (u16*)src + 4 * iy + 16);
					src += 64;
                }
        }
        return PC_TEX_FMT_BGRA32;
    case GX_TF_CMPR:  // speed critical
        // The metroid games use this format almost exclusively.
		{
#if 0   // TODO - currently does not handle transparency correctly and causes problems when texture dimensions are not multiples of 8
            // 11111111 22222222 55555555 66666666
            // 33333333 44444444 77777777 88888888
			for (int y = 0; y < height; y += 8)
			{
                for (int x = 0; x < width; x += 8)
                {
					copyDXTBlock(dst+(y/2)*width+x*2, src);
					src += 8;
					copyDXTBlock(dst+(y/2)*width+x*2+8, src);
					src += 8;
					copyDXTBlock(dst+(y/2+2)*width+x*2, src);
					src += 8;
					copyDXTBlock(dst+(y/2+2)*width+x*2+8, src);
					src += 8;
				}
			}
			return PC_TEX_FMT_DXT1;
#else
			for (int y = 0; y < height; y += 8)
			{
                for (int x = 0; x < width; x += 8)
                {
                    decodeDXTBlock((u32*)dst + y * width + x, (DXTBlock*)src, width);
                                        src += sizeof(DXTBlock);
                    decodeDXTBlock((u32*)dst + y * width + x + 4, (DXTBlock*)src, width);
                                        src += sizeof(DXTBlock);
                    decodeDXTBlock((u32*)dst + (y + 4) * width + x, (DXTBlock*)src, width);
                                        src += sizeof(DXTBlock);
                    decodeDXTBlock((u32*)dst + (y + 4) * width + x + 4, (DXTBlock*)src, width);
                                        src += sizeof(DXTBlock);
                }
			}
#endif
			return PC_TEX_FMT_BGRA32;
		}
    }
*/
	// The "copy" texture formats, too?
    return PC_TEX_FMT_NONE;
}

