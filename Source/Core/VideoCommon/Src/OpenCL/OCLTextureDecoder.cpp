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

#include "OCLTextureDecoder.h"

#include "OpenCL.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
    
struct sDecoders
{
    cl_program program;                 // compute program
    cl_kernel kernel;                   // compute kernel
};

const char *Kernel = "                                        \n\
kernel void DecodeI8(global uchar *dst,                       \n\
                     const global uchar *src, int width)      \n\
{                                                             \n\
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 8;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        vstore8(vload8(srcOffset, src),                       \n\
                0, dst + ((y + iy)*width + x));               \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeIA8(global ushort *dst,                     \n\
                      const global ushort *src, int width)    \n\
{                                                             \n\
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 4;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        vstore4(vload4(srcOffset, src),                       \n\
                0, dst + ((y + iy)*width + x));               \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
ushort swapbytes(ushort x) {                                  \n\
    return (x & 0xf00f) | ((x >> 4) & 0x00f0) |               \n\
           ((x << 4) & 0x0f00);                               \n\
}                                                             \n\
                                                              \n\
kernel void DecodeIA4(global ushort *dst,                     \n\
                     const global uchar *src, int width)      \n\
{                                                             \n\
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 8;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        uchar8 val = vload8(srcOffset, src);                  \n\
        ushort8 res = val.s0011223344556677;                  \n\
        res.s0 = swapbytes(res.s0);                           \n\
        res.s1 = swapbytes(res.s1);                           \n\
        res.s2 = swapbytes(res.s2);                           \n\
        res.s3 = swapbytes(res.s3);                           \n\
        res.s4 = swapbytes(res.s4);                           \n\
        res.s5 = swapbytes(res.s5);                           \n\
        res.s6 = swapbytes(res.s6);                           \n\
        res.s7 = swapbytes(res.s7);                           \n\
        vstore8(res, 0, dst + ((y + iy)*width + x));          \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
";

sDecoders Decoders[] = { {NULL, NULL},
						 {NULL, NULL}, 
						 {NULL, NULL},
};

bool g_Inited = false;

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
    cl_int err;
	cl_kernel kernelToRun = Decoders[0].kernel;
	int sizeOfDst = sizeof(u8), sizeOfSrc = sizeof(u8), xSkip, ySkip;
	PC_TexFormat formatResult;
	cl_mem clsrc, cldst;                    // texture buffer memory objects
    if(!g_Inited)
    {
		if(!OpenCL::Initialize())
			return PC_TEX_FMT_NONE;

		
		Decoders[0].program = OpenCL::CompileProgram(Kernel);

		Decoders[0].kernel = OpenCL::CompileKernel(Decoders[0].program, "DecodeI8");
		Decoders[1].kernel = OpenCL::CompileKernel(Decoders[0].program, "DecodeIA4");
		Decoders[2].kernel = OpenCL::CompileKernel(Decoders[0].program, "DecodeIA8");

		g_Inited = true;
	}
    switch(texformat)
	{
		case GX_TF_IA4:
			// Maybe a cleaner way is needed
			kernelToRun = Decoders[1].kernel;
			sizeOfSrc = sizeof(u8);
			sizeOfDst = sizeof(u16);
			xSkip = 8;
			ySkip = 4;
			formatResult = PC_TEX_FMT_IA4_AS_IA8;
			break;
		case GX_TF_I8:
			kernelToRun = Decoders[0].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u8);
			xSkip = 8;
			ySkip = 4;
			formatResult = PC_TEX_FMT_I8;
			break;
		case GX_TF_IA8:
			kernelToRun = Decoders[2].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u16);
			xSkip = 4;
			ySkip = 4;
			formatResult = PC_TEX_FMT_IA8;
			break;
		default:
			return PC_TEX_FMT_NONE;
	}

	// TODO: Optimize
	clsrc = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, width * height * sizeOfSrc, (void *)src, NULL);
	cldst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, width * height * sizeOfDst, NULL, NULL);

	clSetKernelArg(kernelToRun, 0, sizeof(cl_mem), &cldst);
	clSetKernelArg(kernelToRun, 1, sizeof(cl_mem), &clsrc);
	clSetKernelArg(kernelToRun, 2, sizeof(cl_int), &width);

	size_t global[] = { width / xSkip, height / ySkip };

	// No work-groups for now
	/*
	size_t local;
	err = clGetKernelWorkGroupInfo(kernelToRun, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
	if(err)
		PanicAlert("Error obtaining work-group information");
	*/

	err = clEnqueueNDRangeKernel(OpenCL::GetCommandQueue(), kernelToRun, 2, NULL, global, NULL, 0, NULL, NULL);
	if(err)
		PanicAlert("Error queueing kernel");

	clFinish(OpenCL::GetCommandQueue());
	
	clEnqueueReadBuffer(OpenCL::GetCommandQueue(), cldst, CL_TRUE, 0, width * height * sizeOfDst, dst, 0, NULL, NULL);

	clReleaseMemObject(clsrc);
	clReleaseMemObject(cldst);
	
	return formatResult;
#else
	return PC_TEX_FMT_NONE;
#endif

  /*  switch (texformat)
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
			return PC_TEX_FMT_BGRA32;
		}
    }
*/
	// The "copy" texture formats, too?
    return PC_TEX_FMT_NONE;
}

