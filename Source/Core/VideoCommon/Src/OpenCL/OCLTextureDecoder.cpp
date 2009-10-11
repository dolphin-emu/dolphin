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
	cl_mem src, dst;                    // texture buffer memory objects
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

sDecoders Decoders[] = { {NULL, NULL, NULL, NULL},
						 {NULL, NULL, NULL, NULL}, 
};

bool g_Inited = false;

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
    cl_int err;
	cl_kernel kernelToRun = Decoders[0].kernel;
	int sizeOfDst = sizeof(u8);
    if(!g_Inited)
    {
		if(!OpenCL::Initialize())
			return PC_TEX_FMT_NONE;

		
		Decoders[0].program = OpenCL::CompileProgram(Kernel);
		Decoders[0].kernel = OpenCL::CompileKernel(Decoders[0].program, "DecodeI8");
		Decoders[1].kernel = OpenCL::CompileKernel(Decoders[0].program, "DecodeIA4");

		g_Inited = true;
	}
    switch(texformat)
	{
		case GX_TF_IA4:
			// Maybe a cleaner way is needed
			kernelToRun = Decoders[1].kernel;
			sizeOfDst = sizeof(u16);
		case GX_TF_I8:
			{
				// TODO: Optimize
				//PanicAlert("Really calling the OCL version");
				Decoders[0].src = clCreateBuffer(OpenCL::GetContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, width * height * sizeof(u8), (void *)src, NULL);
				Decoders[0].dst = clCreateBuffer(OpenCL::GetContext(), CL_MEM_WRITE_ONLY, width * height * sizeOfDst, NULL, NULL);

				clSetKernelArg(kernelToRun, 0, sizeof(cl_mem), &Decoders[0].dst);
				clSetKernelArg(kernelToRun, 1, sizeof(cl_mem), &Decoders[0].src);
				clSetKernelArg(kernelToRun, 2, sizeof(cl_int), &width);

				size_t global[] = { width / 8, height / 4 };

				// No work-groups for now
				/*
				size_t local;
				err = clGetKernelWorkGroupInfo(Decoders[0].kernel, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
				if(err)
					PanicAlert("Error obtaining work-group information");
				*/

				err = clEnqueueNDRangeKernel(OpenCL::GetCommandQueue(), kernelToRun, 2 , NULL, global, NULL, 0, NULL, NULL);
				if(err)
					PanicAlert("Error queueing kernel");

				clFinish(OpenCL::GetCommandQueue());
				
				clEnqueueReadBuffer(OpenCL::GetCommandQueue(), Decoders[0].dst, CL_TRUE, 0, width * height * sizeOfDst, dst, 0, NULL, NULL);

				clReleaseMemObject(Decoders[0].src);
				clReleaseMemObject(Decoders[0].dst);
				
				/*
				for (int y = 0; y < height; y += 4)
					for (int x = 0; x < width; x += 8)
						for (int iy = 0; iy < 4; iy++, src += 8)
							memcpy(dst + (y + iy)*width+x, src, 8);
				*/
				if(texformat == GX_TF_I8)
					return PC_TEX_FMT_I8;
				else
					return PC_TEX_FMT_IA4_AS_IA8;
			}
				/* IA4:
				for (int y = 0; y < height; y += 4)
					for (int x = 0; x < width; x += 8)
						for (int iy = 0; iy < 4; iy++, src += 8)
							for (int x = 0; x < 8; x++)
							{
								const u8 val = src[x];
								u8 a = Convert4To8(val >> 4);
								u8 l = Convert4To8(val & 0xF);
								dst[x] = (a << 8) | l;
							}
				*/

		default:
			return PC_TEX_FMT_NONE;
	}
#else
	return PC_TEX_FMT_NONE;
#endif
	/* OLD CODE
    switch(texformat)
    {
        case GX_TF_I8:
		{
			size_t global = 0;                      // global domain size for our calculation
			size_t local = 0;                       // local domain size for our calculation
            printf("width %d, height %d\n", width, height);
            // Create the input and output arrays in device memory for our calculation
            //
            cl_mem _dst = clCreateBuffer(OpenCL::g_context, CL_MEM_WRITE_ONLY, sizeof(unsigned char) * width * height, NULL, NULL);
            if (!dst)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }  
             cl_mem _src = clCreateBuffer(OpenCL::g_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(unsigned char) * width * height, (void*)src, NULL);
            if (!src)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }      
            // Set the arguments to our compute kernel
            //
            err = 0;
            err  = clSetKernelArg(Decoders[0].kernel, 0, sizeof(cl_mem), &_dst);
            err |= clSetKernelArg(Decoders[0].kernel, 1, sizeof(cl_mem), &_src);
            err |= clSetKernelArg(Decoders[0].kernel, 2, sizeof(cl_int), &width);
            err |= clSetKernelArg(Decoders[0].kernel, 3, sizeof(cl_int), &height);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
            }
            
			// Get the maximum work group size for executing the kernel on the device
            //
            err = clGetKernelWorkGroupInfo(Decoders[0].kernel, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &local, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to retrieve kernel work group info! %d\n", err);
                local = 64;
            } 
			
            // Execute the kernel over the entire range of our 1d input data set
            // using the maximum number of work group items for this device
            //
            global = width * height;
            err = clEnqueueNDRangeKernel(OpenCL::g_cmdq, Decoders[0].kernel, 1, NULL, &global, &local, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to execute kernel! %d\n", err);
                return PC_TEX_FMT_NONE;
            }

            // Wait for the command commands to get serviced before reading back results
            //
            clFinish(OpenCL::g_cmdq);

            // Read back the results from the device to verify the output
            //
            err = clEnqueueReadBuffer( OpenCL::g_cmdq, _dst, CL_TRUE, 0, sizeof(unsigned char) * width * height, dst, 0, NULL, NULL );  
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to read output array! %d\n", err);
                exit(1);
            }
			clReleaseMemObject(_dst);
			clReleaseMemObject(_src);
		}
		return PC_TEX_FMT_I8;
        break;
        default:
            return PC_TEX_FMT_NONE;
    }
	// TODO: clEnqueueNDRangeKernel
*/

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

