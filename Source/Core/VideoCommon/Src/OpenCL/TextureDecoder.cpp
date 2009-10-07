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

#include "OpenCL.h"
#include <CL/cl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
    
    
struct sDecoders
{
    cl_program program;                 // compute program
    cl_kernel kernel;                   // compute kernel
    const char **cKernel;
};

const char *Kernel = "         \n                     		\
__kernel void Decode(__global unsigned char *dst,               \n		\
		     const __global unsigned char *src,       \n        		\
		     const __global int width, const __global int height)       \n          		\
{                                    \n               		\
	int x = get_global_id(0) % width, y = get_global_id(0) / width;   \n		\
	if((y % 4) == 0 && (x % 8) == 0) \n \
	{ \n \
	int srcOffset = (x * 4) + (y * width); \n \
//			for (int y = 0; y < height; y += 4) \n \
//				for (int x = 0; x < width; x += 8) \n \
	for (int iy = 0; iy < 4; iy++, srcOffset += 8) \n\
	{ \n \
		dst[(y + iy)*width + x] = src[srcOffset]; \n \
		dst[(y + iy)*width + x + 1] = src[srcOffset + 1]; \n \
		dst[(y + iy)*width + x + 2] = src[srcOffset + 2]; \n \
		dst[(y + iy)*width + x + 3] = src[srcOffset + 3]; \n \
		dst[(y + iy)*width + x + 4] = src[srcOffset + 4]; \n \
		dst[(y + iy)*width + x + 5] = src[srcOffset + 5]; \n \
		dst[(y + iy)*width + x + 6] = src[srcOffset + 6]; \n \
		dst[(y + iy)*width + x + 7] = src[srcOffset + 7]; \n \
	} \n \
} \n \
}\n";
//						memcpy(dst + (y + iy)*width+x, src, 8);			
const char *KernelOld = "         \n                     		\
__kernel void Decode(__global uchar *dst,               \n		\
		     const __global uchar *src,       \n        		\
		     int width, int height)       \n          		\
{  \n \
    dst[get_global_id(0)] = 0xFF; \n \
} \n ";
sDecoders Decoders[] = { {NULL, NULL, &Kernel}, };

bool g_Inited = false;

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
    int err;
    if(!g_Inited)
    {
		g_Inited = true;

#if defined(HAVE_OPENCL) && HAVE_OPENCL
		// TODO: Switch this over to the OpenCl.h backend
	    // Create the compute program from the source buffer
		//
		
		Decoders[0].program = clCreateProgramWithSource(OpenCL::g_context, 1, (const char **) & Kernel, NULL, &err);
		if (!Decoders[0].program)
		{
			printf("Error: Failed to create compute program!\n");
			return PC_TEX_FMT_NONE;
		}

		// Build the program executable
		//
		err = clBuildProgram(Decoders[0].program , 0, NULL, NULL, NULL, NULL);
		if (err != CL_SUCCESS)
		{
			size_t len;
			char buffer[2048];

			printf("Error: Failed to build program executable!\n");
			clGetProgramBuildInfo(Decoders[0].program , OpenCL::device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
			printf("%s\n", buffer);
			exit(1);
		}

		// Create the compute kernel in the program we wish to run
		//
		Decoders[0].kernel = clCreateKernel(Decoders[0].program, "Decode", &err);
		if (!Decoders[0].kernel || err != CL_SUCCESS)
		{
			printf("Error: Failed to create compute kernel!\n");
			exit(1);
		} 
    
#endif
	}
	    /*switch(texformat)
		{
			case GX_TF_I8:
			{
				int srcOffset = 0;
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 4; iy++, srcOffset += 8)
					{
						printf("x: %d y: %d offset: %d\n", x, y, srcOffset);
						memcpy(dst + (y + iy)*width+x, src + srcOffset, 8);
					}
			return PC_TEX_FMT_I8;
			}
			break;
			default:
			return PC_TEX_FMT_NONE;
		}
		//return PC_TEX_FMT_NONE;*/
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

