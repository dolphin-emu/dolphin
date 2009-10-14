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
    const char name[256];               // kernel name
    cl_kernel kernel;                   // compute kernel
};

const char *Kernel = "                                        \n\
kernel void DecodeI4(global uchar *dst,                       \n\
                     const global uchar *src, int width)      \n\
{                                                             \n\
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;   \n\
    int srcOffset = x + y * width / 8;                        \n\
    for (int iy = 0; iy < 8; iy++)                            \n\
    {                                                         \n\
        uchar4 val = vload4(srcOffset, src);                  \n\
        uchar8 res;                                           \n\
        res.even = (val >> 4) & 0x0F;                         \n\
        res.odd = val & 0x0F;                                 \n\
        res |= res << 4;                                      \n\
        vstore8(res, 0, dst + ((y + iy)*width + x));          \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
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
kernel void DecodeIA8(global uchar *dst,                      \n\
                      const global uchar *src, int width)     \n\
{                                                             \n\
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 4;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        uchar8 val = vload8(srcOffset, src);                  \n\
        uchar8 res;                                           \n\
        res.odd = val.even;                                   \n\
        res.even = val.odd;                                   \n\
        vstore8(res, 0, dst + ((y + iy)*width + x) * 2);      \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeIA4(global uchar *dst,                      \n\
                     const global uchar *src, int width)      \n\
{                                                             \n\
    int x = get_global_id(0) * 8, y = get_global_id(1) * 4;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 8;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        uchar8 val = vload8(srcOffset, src);                  \n\
        uchar16 res;                                          \n\
        res.odd = (val >> 4) & 0x0F;                          \n\
        res.even  = val & 0x0F;                               \n\
        res |= res << 4;                                      \n\
        vstore16(res, 0, dst + ((y + iy)*width + x) * 2);     \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeRGBA8(global uchar *dst,                    \n\
                      const global uchar *src, int width)     \n\
{                                                             \n\
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;   \n\
    int srcOffset = (x * 2) + (y * width) / 2;                \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        uchar8 ar = vload8(srcOffset, src);                   \n\
        uchar8 gb = vload8(srcOffset + 4, src);               \n\
        uchar16 res;                                          \n\
        res.even.even = gb.odd;                               \n\
        res.even.odd = ar.odd;                                \n\
        res.odd.even = gb.even;                               \n\
        res.odd.odd = ar.even;                                \n\
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);     \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeRGB565(global ushort *dst,                  \n\
                         const global ushort *src, int width) \n\
{                                                             \n\
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;   \n\
    int srcOffset = x + (y * width) / 4;                      \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        ushort4 val = vload4(srcOffset, src);                 \n\
        val = (val >> 8) | (val << 8);                        \n\
        vstore4(val, 0, dst + ((y + iy)*width + x));          \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeRGB5A3(global uchar *dst,                   \n\
                         const global uchar *src, int width)  \n\
{                                                             \n\
    int x = get_global_id(0) * 4, y = get_global_id(1) * 4;   \n\
    int srcOffset = x + (y * width) / 4;                      \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        ushort8 val = convert_ushort8(vload8(srcOffset, src));\n\
        ushort4 vs = val.odd | (val.even << 8);               \n\
        uchar16 resNoAlpha;                                   \n\
        resNoAlpha.odd.odd = (uchar4)(0xFF);                  \n\
        resNoAlpha.even.odd = convert_uchar4(vs >> 7) & 0xF8; \n\
        resNoAlpha.odd.even = convert_uchar4(vs >> 2) & 0xF8; \n\
        resNoAlpha.even.even = convert_uchar4(vs << 3) & 0xF8;\n\
        // Better but cause color bleeding                    \n\
        //resNoAlpha |= resNoAlpha >> 5;                      \n\
        uchar16 resAlpha;                                     \n\
        resAlpha.even.odd = convert_uchar4(vs >> 8) & 0x0F;   \n\
        resAlpha.odd.even = convert_uchar4(vs >> 4) & 0x0F;   \n\
        resAlpha.even.even = convert_uchar4(vs) & 0x0F;       \n\
        resAlpha |= resNoAlpha << 4;                          \n\
        resAlpha.odd.odd = convert_uchar4(vs >> 7) & 0xE0;    \n\
        resAlpha.odd.odd |= (resAlpha.odd.odd >> 3)           \n\
                            | (resAlpha.odd.odd >> 6);        \n\
        uchar16 choice = (uchar16)((uchar4)(vs.s0 >> 8),      \n\
                                   (uchar4)(vs.s1 >> 8),      \n\
                                   (uchar4)(vs.s2 >> 8),      \n\
                                   (uchar4)(vs.s3 >> 8));     \n\
        uchar16 res;                                          \n\
        res = select(resAlpha, resNoAlpha, choice);           \n\
        vstore16(res, 0, dst + ((y + iy)*width + x) * 4);     \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
                                                              \n\
kernel void DecodeDXT(global ulong *dst,                      \n\
                      const global ulong *src, int width)     \n\
{   // TODO: PLEASE NOTE THAT THIS CODE DOES NOT WORK         \n\
    int x = get_global_id(0) * 8, y = get_global_id(1) * 8;   \n\
    int srcOffset = ((x * 4) + (y * width)) / 8;              \n\
    for (int iy = 0; iy < 4; iy++)                            \n\
    {                                                         \n\
        vstore8(vload8(srcOffset, src),                       \n\
                0, dst + ((y + iy)*width + x));               \n\
        srcOffset++;                                          \n\
    }                                                         \n\
}                                                             \n\
";

cl_program g_program;
// NULL terminated set of kernels
sDecoders Decoders[] = {
{"DecodeI4", NULL},
{"DecodeI8", NULL},
{"DecodeIA4", NULL}, 
{"DecodeIA8", NULL},
{"DecodeRGBA8", NULL},
{"DecodeRGB565", NULL},
{"DecodeRGB5A3", NULL},
{"DecodeDXT", NULL},
{"", NULL},
};

bool g_Inited = false;
cl_mem g_clsrc, g_cldst;                    // texture buffer memory objects

void TexDecoder_OpenCL_Initialize() {
#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(!g_Inited)
    {
		if(!OpenCL::Initialize())
			return;

		
		g_program = OpenCL::CompileProgram(Kernel);

		int i = 0;
		while(strlen(Decoders[i].name) > 0) {
			Decoders[i].kernel = OpenCL::CompileKernel(g_program, Decoders[i].name);
			i++;
		}

		// Allocating maximal Wii texture size in advance, so that we don't have to allocate/deallocate per texture
		g_clsrc = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY , 1024 * 1024 * sizeof(u32), NULL, NULL);
		g_cldst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, 1024 * 1024 * sizeof(u32), NULL, NULL);

		g_Inited = true;
	}
#endif
}

void TexDecoder_OpenCL_Shutdown() {
#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(g_clsrc)
		clReleaseMemObject(g_clsrc);

	if(g_cldst)
		clReleaseMemObject(g_cldst);
#endif
}

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
    cl_int err;
	cl_kernel kernelToRun = Decoders[0].kernel;
	float sizeOfDst = sizeof(u8), sizeOfSrc = sizeof(u8), xSkip, ySkip;
	PC_TexFormat formatResult;
  
    switch(texformat)
	{
		case GX_TF_I4:
            kernelToRun = Decoders[0].kernel;
			sizeOfSrc = sizeof(u8) / 2.0f;
			sizeOfDst = sizeof(u8);
			xSkip = 8;
			ySkip = 8;
            formatResult = PC_TEX_FMT_I4_AS_I8;
            break;
		case GX_TF_I8:
			kernelToRun = Decoders[1].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u8);
			xSkip = 8;
			ySkip = 4;
			formatResult = PC_TEX_FMT_I8;
			break;
		case GX_TF_IA4:
			// Maybe a cleaner way is needed
			kernelToRun = Decoders[2].kernel;
			sizeOfSrc = sizeof(u8);
			sizeOfDst = sizeof(u16);
			xSkip = 8;
			ySkip = 4;
			formatResult = PC_TEX_FMT_IA4_AS_IA8;
			break;
		case GX_TF_IA8:
			kernelToRun = Decoders[3].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u16);
			xSkip = 4;
			ySkip = 4;
			formatResult = PC_TEX_FMT_IA8;
			break;
		case GX_TF_RGBA8:
			kernelToRun = Decoders[4].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u32);
			xSkip = 4;
			ySkip = 4;
			formatResult = PC_TEX_FMT_BGRA32;
			break;
		case GX_TF_RGB565:
			kernelToRun = Decoders[5].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u16);
			xSkip = 4;
			ySkip = 4;
            formatResult = PC_TEX_FMT_RGB565;
			break;
        case GX_TF_RGB5A3:
			kernelToRun = Decoders[6].kernel;
			sizeOfSrc = sizeof(u16);
            sizeOfDst = sizeof(u32);
			xSkip = 4;
			ySkip = 4;
			formatResult = PC_TEX_FMT_BGRA32;
			break;
		case GX_TF_CMPR:
			return PC_TEX_FMT_NONE; // <-- TODO: Fix CMPR
			kernelToRun = Decoders[7].kernel;
			sizeOfSrc = sizeOfDst = sizeof(u32);
			xSkip = 8;
			ySkip = 8;
			formatResult = PC_TEX_FMT_BGRA32;
			break;
		default:
			return PC_TEX_FMT_NONE;
	}

	clEnqueueWriteBuffer(OpenCL::GetCommandQueue(), g_clsrc, CL_TRUE, 0, (size_t)(width * height * sizeOfSrc), src, 0, NULL, NULL);

	clSetKernelArg(kernelToRun, 0, sizeof(cl_mem), &g_cldst);
	clSetKernelArg(kernelToRun, 1, sizeof(cl_mem), &g_clsrc);
	clSetKernelArg(kernelToRun, 2, sizeof(cl_int), &width);

	size_t global[] = { (size_t)(width / xSkip), (size_t)(height / ySkip) };

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
	
	clEnqueueReadBuffer(OpenCL::GetCommandQueue(), g_cldst, CL_TRUE, 0, (size_t)(width * height * sizeOfDst), dst, 0, NULL, NULL);
	
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

