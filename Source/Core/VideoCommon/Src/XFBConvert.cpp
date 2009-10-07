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

#if _WIN32
#include <intrin.h>
#endif

#include "OpenCL.h"

#include <xmmintrin.h>

#include "XFBConvert.h"
#include "Common.h"

namespace {

const __m128i _bias1 = _mm_set_epi32(128/2 << 16,        0, 128/2 << 16, 16 << 16);
const __m128i _bias2 = _mm_set_epi32(128/2 << 16, 16 << 16, 128/2 << 16,        0);

__m128i _y[256];
__m128i _u[256];
__m128i _v[256];

__m128i _r1[256];
__m128i _r2[256];
__m128i _g1[256];
__m128i _g2[256];
__m128i _b1[256];
__m128i _b2[256];

}  // namespace
#if defined(HAVE_OPENCL) && HAVE_OPENCL
bool Inited = false;

	cl_kernel To_kernel;
	cl_program To_program;
	cl_kernel From_kernel;
	cl_program From_program;
	
const char *__ConvertFromXFB = "int bound(int i) \n \
{ \n \
        return (i>255)?255:((i<0)?0:i); \n \
} \n \
 \n \
void yuv2rgb(int y, int u, int v, int &r, int &g, int &b) \n \
{ \n \
        b = bound((76283*(y - 16) + 132252*(u - 128))>>16); \n \
        g = bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16); //last one u? \n \
        r = bound((76283*(y - 16) + 104595*(v - 128))>>16); \n \
} \n \
 \n \
void ConvertFromXFB(u32 *dst, const u8* _pXFB) \n \
{ \n \
        const unsigned char *src = _pXFB; \n \
		int id = get_global_id(0); \n \
		int srcOffset = id * 4; \n \
		int dstOffset = id; \n \
        u32 numBlocks = (width * height) / 2; \n \
 \n \
                int Y1 = src[srcOffset]; \n \
                int U  = src[srcOffset + 1]; \n \
                int Y2 = src[srcOffset + 2]; \n \
                int V  = src[srcOffset + 3]; \n \
 \n \
                int r, g, b; \n \
                yuv2rgb(Y1,U,V, r,g,b); \n \
                dst[dstOffset] = 0xFF000000 | (r<<16) | (g<<8) | (b); \n \
                yuv2rgb(Y2,U,V, r,g,b); \n \
                dst[dstOffset + 1] = 0xFF000000 | (r<<16) | (g<<8) | (b); \n \
} \n";

const char  *__ConvertToXFB = "__kernel void ConvertToXFB(__global unsigned int *dst, __global const unsigned char* _pEFB) \n \
{ \n \
        const unsigned char *src = _pEFB;\n \
		int id = get_global_id(0);\n \
		int srcOffset = id * 8; \n \
		\n \
		int y1 = (((16843 * src[srcOffset]) + (33030 * src[srcOffset + 1]) + (6423 * src[srcOffset + 2])) >> 16) + 16;          \n \
		int u1 = ((-(9699 * src[srcOffset]) - (19071 * src[srcOffset + 1]) + (28770 * src[srcOffset + 2])) >> 16) + 128;\n \
		srcOffset += 4;\n \
		\n \
		int y2 = (((16843 * src[srcOffset]) + (33030 * src[srcOffset + 1]) + (6423 * src[srcOffset + 2])) >> 16) + 16;\n \
		int v2 = (((28770 * src[srcOffset]) - (24117 * src[srcOffset + 1]) - (4653 * src[srcOffset + 2])) >> 16) + 128;\n \
		\n \
		dst[id] = (v2 << 24) | (y2 << 16) | (u1 << 8) | (y1);    \n \
} \n ";

void InitKernels()
{
	

	From_program = OpenCL::CompileProgram(__ConvertFromXFB);
	From_kernel = OpenCL::CompileKernel(From_program, "ConvertFromXFB");
	
	To_program = OpenCL::CompileProgram(__ConvertToXFB);
	To_kernel = OpenCL::CompileKernel(To_program, "ConvertToXFB");
	Inited = true;
}
#endif
void InitXFBConvTables() 
{
	for (int i = 0; i < 256; i++)
	{
		_y[i] = _mm_set_epi32(0xFFFFFFF,     76283*(i - 16),      76283*(i - 16),     76283*(i - 16));
		_u[i] = _mm_set_epi32(        0,                  0,  -25624 * (i - 128), 132252 * (i - 128));
		_v[i] = _mm_set_epi32(        0, 104595 * (i - 128),  -53281 * (i - 128),                  0);

		_r1[i] = _mm_add_epi32(_mm_set_epi32( 28770 * i / 2,          0,  -9699 * i / 2, 16843 * i),
							   _bias1);
		_g1[i] = _mm_set_epi32(-24117 * i / 2,          0, -19071 * i / 2, 33030 * i);
		_b1[i] = _mm_set_epi32( -4653 * i / 2,          0,  28770 * i / 2,  6423 * i);
										   									
		_r2[i] = _mm_add_epi32(_mm_set_epi32( 28770 * i / 2,  16843 * i,  -9699 * i / 2,         0),
							   _bias2);
		_g2[i] = _mm_set_epi32(-24117 * i / 2,  33030 * i, -19071 * i / 2,         0);
		_b2[i] = _mm_set_epi32( -4653 * i / 2,   6423 * i,  28770 * i / 2,         0);
	}
}

void ConvertFromXFB(u32 *dst, const u8* _pXFB, int width, int height)
{
	if (((size_t)dst & 0xF) != 0) {
		PanicAlert("ConvertFromXFB - unaligned destination");
	}	
		const unsigned char *src = _pXFB;
	u32 numBlocks = ((width * height) / 2) / 2;
	#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(!Inited)
		InitKernels();
	int err;
	
			size_t global = 0;                      // global domain size for our calculation
			size_t local = 0;                       // local domain size for our calculation
            printf("width %d, height %d\n", width, height);
            // Create the input and output arrays in device memory for our calculation
            //
            cl_mem _dst = clCreateBuffer(OpenCL::g_context, CL_MEM_WRITE_ONLY, sizeof(unsigned int) * numBlocks, NULL, NULL);
            if (!dst)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }  
            cl_mem _src = clCreateBuffer(OpenCL::g_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(unsigned char) * width * height, (void*)_pXFB, NULL);
            if (!src)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }      
            // Set the arguments to our compute kernel
            //
            err = 0;
            err  = clSetKernelArg(From_kernel, 0, sizeof(cl_mem), &_dst);
            err |= clSetKernelArg(From_kernel, 1, sizeof(cl_mem), &_src);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
            }
            
        // Get the maximum work group size for executing the kernel on the device
            //
            err = clGetKernelWorkGroupInfo(From_kernel, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &local, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to retrieve kernel work group info! %d\n", err);
				local = 64;
            } 
			
            // Execute the kernel over the entire range of our 1d input data set
            // using the maximum number of work group items for this device
            //
            global = numBlocks;
			if(global < local)
			{
				// Global can't be less than local
			}
            err = clEnqueueNDRangeKernel(OpenCL::g_cmdq, From_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to execute kernel! %d\n", err);
                return;
            }

            // Wait for the command commands to get serviced before reading back results
            //
            clFinish(OpenCL::g_cmdq);

            // Read back the results from the device to verify the output
            //
            err = clEnqueueReadBuffer( OpenCL::g_cmdq, _dst, CL_TRUE, 0, sizeof(unsigned int) * numBlocks, dst, 0, NULL, NULL );  
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to read output array! %d\n", err);
                exit(1);
            }
			clReleaseMemObject(_dst);
			clReleaseMemObject(_src);
	#else
	for (u32 i = 0; i < numBlocks; i++)
	{
		__m128i y1 = _y[src[0]];
		__m128i u  = _u[src[1]];
		__m128i y2 = _y[src[2]];
		__m128i v  = _v[src[3]];
		__m128i y1_2 = _y[src[4+0]];
		__m128i u_2  = _u[src[4+1]];
		__m128i y2_2 = _y[src[4+2]];
		__m128i v_2  = _v[src[4+3]];

		__m128i c1 = _mm_srai_epi32(_mm_add_epi32(y1, _mm_add_epi32(u, v)), 16);
		__m128i c2 = _mm_srai_epi32(_mm_add_epi32(y2, _mm_add_epi32(u, v)), 16);
		__m128i c3 = _mm_srai_epi32(_mm_add_epi32(y1_2, _mm_add_epi32(u_2, v_2)), 16);
		__m128i c4 = _mm_srai_epi32(_mm_add_epi32(y2_2, _mm_add_epi32(u_2, v_2)), 16);

		__m128i four_dest = _mm_packus_epi16(_mm_packs_epi32(c1, c2), _mm_packs_epi32(c3, c4));
		_mm_store_si128((__m128i *)dst, four_dest);
		dst += 4;
		src += 8;
	}
	#endif
}


void ConvertToXFB(u32 *dst, const u8* _pEFB, int width, int height)
{
	const unsigned char *src = _pEFB;

	u32 numBlocks = ((width * height) / 2) / 4;
	if (((size_t)dst & 0xF) != 0) {
		PanicAlert("ConvertToXFB - unaligned XFB");
	}
	#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(!Inited)
		InitKernels();

	int err;
	
			size_t global = 0;                      // global domain size for our calculation
			size_t local = 0;                       // local domain size for our calculation
            printf("width %d, height %d\n", width, height);
            // Create the input and output arrays in device memory for our calculation
            //
            cl_mem _dst = clCreateBuffer(OpenCL::g_context, CL_MEM_WRITE_ONLY, sizeof(unsigned int) * numBlocks, NULL, NULL);
            if (!dst)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }  
            cl_mem _src = clCreateBuffer(OpenCL::g_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(unsigned char) * width * height, (void*)_pEFB, NULL);
            if (!src)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }      
            // Set the arguments to our compute kernel
            //
            err = 0;
            err  = clSetKernelArg(To_kernel, 0, sizeof(cl_mem), &_dst);
            err |= clSetKernelArg(To_kernel, 1, sizeof(cl_mem), &_src);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
            }
            
        // Get the maximum work group size for executing the kernel on the device
            //
            err = clGetKernelWorkGroupInfo(To_kernel, OpenCL::device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &local, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to retrieve kernel work group info! %d\n", err);
                local = 64;
            } 
			
            // Execute the kernel over the entire range of our 1d input data set
            // using the maximum number of work group items for this device
            //
            global = numBlocks;
			if(global < local)
			{
				// Global can't be less than local
			}
            err = clEnqueueNDRangeKernel(OpenCL::g_cmdq, To_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to execute kernel! %d\n", err);
                return;
            }

            // Wait for the command commands to get serviced before reading back results
            //
            clFinish(OpenCL::g_cmdq);

            // Read back the results from the device to verify the output
            //
            err = clEnqueueReadBuffer( OpenCL::g_cmdq, _dst, CL_TRUE, 0, sizeof(unsigned int) * numBlocks, dst, 0, NULL, NULL );  
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to read output array! %d\n", err);
                exit(1);
            }
			clReleaseMemObject(_dst);
			clReleaseMemObject(_src);
	#else
	for (u32 i = 0; i < numBlocks; i++)
	{
		__m128i yuyv0 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])),
				_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]]))), 16);
		src += 8;
		__m128i yuyv1 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])),
				_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]]))), 16);
		src += 8;
		__m128i yuyv2 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])),
				_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]]))), 16);
		src += 8;
		__m128i yuyv3 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])),
				_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]]))), 16);
		src += 8;
		__m128i four_dest = _mm_packus_epi16(_mm_packs_epi32(yuyv0, yuyv1), _mm_packs_epi32(yuyv2, yuyv3));
		_mm_store_si128((__m128i *)dst, four_dest);
		dst += 4;
	}
	#endif
}

