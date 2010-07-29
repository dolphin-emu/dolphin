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
#include "FileUtil.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <string>

//#define DEBUG_OPENCL

cl_program g_program;

struct sDecoderParameter
{
	const char *name;
	cl_kernel kernel;
	float sizeOfSrc;
	float sizeOfDst;
	int xSkip;
	int ySkip;
	PC_TexFormat format;
};

sDecoderParameter g_DecodeParametersNative[] = {
	/* GX_TF_I4     */ { "DecodeI4",     NULL, 0.5f, 1, 8, 8, PC_TEX_FMT_I4_AS_I8 },
	/* GX_TF_I8     */ { "DecodeI8",     NULL,    1, 1, 8, 4, PC_TEX_FMT_I8 },
	/* GX_TF_IA4    */ { "DecodeIA4",    NULL,    1, 2, 8, 4, PC_TEX_FMT_IA4_AS_IA8 },
	/* GX_TF_IA8    */ { "DecodeIA8",    NULL,    2, 2, 4, 4, PC_TEX_FMT_IA8 },
	/* GX_TF_RGB565 */ { "DecodeRGB565", NULL,    2, 2, 4, 4, PC_TEX_FMT_RGB565 },
	/* GX_TF_RGB5A3 */ { "DecodeRGB5A3", NULL,    2, 4, 4, 4, PC_TEX_FMT_BGRA32 },
	/* GX_TF_RGBA8  */ { "DecodeRGBA8",  NULL,    4, 4, 4, 4, PC_TEX_FMT_BGRA32 },
	/* 7            */ { NULL },
	/* GX_TF_C4     */ { NULL },
	/* GX_TF_C8     */ { NULL },
	/* GX_TF_C14X2  */ { NULL },
	/* B            */ { NULL },
	/* C            */ { NULL },
	/* D            */ { NULL },
	/* GX_TF_CMPR   */ { "DecodeCMPR",   NULL, 0.5f, 4, 8, 8, PC_TEX_FMT_BGRA32 },
};

sDecoderParameter g_DecodeParametersRGBA[] = {
	/* GX_TF_I4     */ { "DecodeI4_RGBA",     NULL, 0.5f, 4, 8, 8, PC_TEX_FMT_RGBA32 },
	/* GX_TF_I8     */ { "DecodeI8_RGBA",     NULL,    1, 4, 8, 4, PC_TEX_FMT_RGBA32 },
	/* GX_TF_IA4    */ { "DecodeIA4_RGBA",    NULL,    1, 4, 8, 4, PC_TEX_FMT_RGBA32 },
	/* GX_TF_IA8    */ { "DecodeIA8_RGBA",    NULL,    2, 4, 4, 4, PC_TEX_FMT_RGBA32 },
	/* GX_TF_RGB565 */ { "DecodeRGB565_RGBA", NULL,    2, 4, 4, 4, PC_TEX_FMT_RGBA32 },
	/* GX_TF_RGB5A3 */ { "DecodeRGB5A3_RGBA", NULL,    2, 4, 4, 4, PC_TEX_FMT_RGBA32 },
	/* GX_TF_RGBA8  */ { "DecodeRGBA8_RGBA",  NULL,    4, 4, 4, 4, PC_TEX_FMT_RGBA32 },
	/* 7            */ { NULL },
	/* GX_TF_C4     */ { NULL },
	/* GX_TF_C8     */ { NULL },
	/* GX_TF_C14X2  */ { NULL },
	/* B            */ { NULL },
	/* C            */ { NULL },
	/* D            */ { NULL },
	/* GX_TF_CMPR   */ { "DecodeCMPR_RGBA",   NULL, 0.5f, 4, 8, 8, PC_TEX_FMT_RGBA32 },
};

bool g_Inited = false;
cl_mem g_clsrc, g_cldst;                    // texture buffer memory objects

void TexDecoder_OpenCL_Initialize()
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(!g_Inited)
	{
		if(!OpenCL::Initialize())
			return;

		std::string code;
		char filename[1024];
		sprintf(filename, "%sOpenCL/TextureDecoder.cl", File::GetUserPath(D_USER_IDX));
		if (!File::ReadFileToString(true, filename, code))
		{
			ERROR_LOG(VIDEO, "Failed to load OpenCL code %s - file is missing?", filename);
			return;
		}

		g_program = OpenCL::CompileProgram(code.c_str());

		for (int i = 0; i <= GX_TF_CMPR; ++i) {
			if (g_DecodeParametersNative[i].name)
				g_DecodeParametersNative[i].kernel =
					OpenCL::CompileKernel(g_program,
					g_DecodeParametersNative[i].name);

			if (g_DecodeParametersRGBA[i].name)
				g_DecodeParametersRGBA[i].kernel =
					OpenCL::CompileKernel(g_program,
					g_DecodeParametersRGBA[i].name);
		}

		// Allocating maximal Wii texture size in advance, so that we don't have to allocate/deallocate per texture
#ifndef DEBUG_OPENCL
		g_clsrc = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY , 1024 * 1024 * sizeof(u32), NULL, NULL);
		g_cldst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, 1024 * 1024 * sizeof(u32), NULL, NULL);
#endif

		g_Inited = true;
	}
#endif
}

void TexDecoder_OpenCL_Shutdown()
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL && !defined(DEBUG_OPENCL)

	clReleaseProgram(g_program);

	for (int i = 0; i < GX_TF_CMPR; ++i) {
		if (g_DecodeParametersNative[i].kernel)
			clReleaseKernel(g_DecodeParametersNative[i].kernel);

		if(g_DecodeParametersRGBA[i].kernel)
			clReleaseKernel(g_DecodeParametersRGBA[i].kernel);
	}

	if(g_clsrc)
		clReleaseMemObject(g_clsrc);

	if(g_cldst)
		clReleaseMemObject(g_cldst);

	g_Inited = false;
#endif
}

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt, bool rgba)
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
	cl_int err;
	sDecoderParameter& decoder = rgba ? g_DecodeParametersRGBA[texformat] : g_DecodeParametersNative[texformat];
	if(!g_Inited || !decoder.name || !decoder.kernel || decoder.format == PC_TEX_FMT_NONE)
		return PC_TEX_FMT_NONE;

#ifdef DEBUG_OPENCL
	g_clsrc = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY , 1024 * 1024 * sizeof(u32), NULL, NULL);
	g_cldst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, 1024 * 1024 * sizeof(u32), NULL, NULL);
#endif

	clEnqueueWriteBuffer(OpenCL::GetCommandQueue(), g_clsrc, CL_TRUE, 0, (size_t)(width * height * decoder.sizeOfSrc), src, 0, NULL, NULL);

	clSetKernelArg(decoder.kernel, 0, sizeof(cl_mem), &g_cldst);
	clSetKernelArg(decoder.kernel, 1, sizeof(cl_mem), &g_clsrc);
	clSetKernelArg(decoder.kernel, 2, sizeof(cl_int), &width);

	size_t global[] = { (size_t)(width / decoder.xSkip), (size_t)(height / decoder.ySkip) };

	// No work-groups for now
	/*
	size_t local;
	err = clGetKernelWorkGroupInfo(kernelToRun, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
	if(err)
		PanicAlert("Error obtaining work-group information");
	*/

	err = clEnqueueNDRangeKernel(OpenCL::GetCommandQueue(), decoder.kernel, 2, NULL, global, NULL, 0, NULL, NULL);
	if(err)
		OpenCL::HandleCLError(err, "Failed to enqueue kernel");

	clFinish(OpenCL::GetCommandQueue());

	clEnqueueReadBuffer(OpenCL::GetCommandQueue(), g_cldst, CL_TRUE, 0, (size_t)(width * height * decoder.sizeOfDst), dst, 0, NULL, NULL);

#ifdef DEBUG_OPENCL
	clReleaseMemObject(g_clsrc);
	clReleaseMemObject(g_cldst);
#endif

	return decoder.format;
#else
	return PC_TEX_FMT_NONE;
#endif
}

