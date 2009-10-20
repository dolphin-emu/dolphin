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
#include <sys/stat.h>
#include <string>

//#define DEBUG_OPENCL

struct sDecoders
{
    const char name[256];               // kernel name
    cl_kernel kernel;                   // compute kernel
};

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
{"DecodeCMPR", NULL},
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

        std::string code;
        char* filename = "User/OpenCL/TextureDecoder.cl";
		if (!File::ReadFileToString(true, filename, code))
		{
			ERROR_LOG(VIDEO, "Failed to load OpenCL code %s - file is missing?", filename);
            return;
		}
		
		g_program = OpenCL::CompileProgram(code.c_str());

		int i = 0;
		while(strlen(Decoders[i].name) > 0) {
			Decoders[i].kernel = OpenCL::CompileKernel(g_program, Decoders[i].name);
			i++;
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

void TexDecoder_OpenCL_Shutdown() {
#if defined(HAVE_OPENCL) && HAVE_OPENCL && !defined(DEBUG_OPENCL)
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
			return PC_TEX_FMT_NONE; // Remove to test CMPR
			kernelToRun = Decoders[7].kernel;
			sizeOfSrc = sizeof(u8) / 2.0f;
            sizeOfDst = sizeof(u32);
			xSkip = 8;
			ySkip = 8;
			formatResult = PC_TEX_FMT_BGRA32;
			break;
		default:
			return PC_TEX_FMT_NONE;
	}

#ifdef DEBUG_OPENCL
	g_clsrc = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY , 1024 * 1024 * sizeof(u32), NULL, NULL);
	g_cldst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, 1024 * 1024 * sizeof(u32), NULL, NULL);
#endif

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
        OpenCL::HandleCLError(err, "Failed to enqueue kernel");

	clFinish(OpenCL::GetCommandQueue());
	
	clEnqueueReadBuffer(OpenCL::GetCommandQueue(), g_cldst, CL_TRUE, 0, (size_t)(width * height * sizeOfDst), dst, 0, NULL, NULL);

#ifdef DEBUG_OPENCL
	clReleaseMemObject(g_clsrc);
	clReleaseMemObject(g_cldst);
#endif

	return formatResult;
#else
	return PC_TEX_FMT_NONE;
#endif

    return PC_TEX_FMT_NONE;
}

