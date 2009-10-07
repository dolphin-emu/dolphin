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

// TODO: Make a more centralized version of this (for now every plugin that will use it will create its own context, which is weird). An object maybe?

#include "OpenCL.h"
#include "Common.h"

namespace OpenCL {
	#if defined(HAVE_OPENCL) && HAVE_OPENCL
	cl_device_id device_id = NULL;
    cl_context g_context = NULL;
    cl_command_queue g_cmdq = NULL;
	#endif

bool g_bInitialized = false;

bool Initialize() {
	if(g_bInitialized)
		return true;

#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(g_context)
		return false;
	int err;                            // error code returned from api calls
	
            
    // Connect to a compute device
    //
    int gpu = 1; // I think we should use CL_DEVICE_TYPE_ALL
    err = clGetDeviceIDs(NULL, gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
    if (err != CL_SUCCESS)
    {
        PanicAlert("Error: Failed to create a device group!");
        return false;
    }
  
    // Create a compute context 
    //
    g_context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!g_context)
    {
        PanicAlert("Error: Failed to create a compute context!");
        return false;
    }

    // Create a command commands
    //
    g_cmdq = clCreateCommandQueue(g_context, device_id, 0, &err);
    if (!g_cmdq)
    {
        PanicAlert("Error: Failed to create a command commands!");
        return false;
    }
	//PanicAlert("Initialized OpenCL fine!");
	g_bInitialized = true;
	return true;
#else
	return false;
#endif
}
#if defined(HAVE_OPENCL) && HAVE_OPENCL
cl_context GetContext() {
	return g_context;
}

cl_command_queue GetCommandQueue() {
	return g_cmdq;
}

cl_program CompileProgram(const char *Kernel) {
		int err;
		cl_program program; 
		program = clCreateProgramWithSource(OpenCL::g_context, 1, (const char **) & Kernel, NULL, &err);
		if (!program)
		{
			printf("Error: Failed to create compute program!");
			return NULL;
		}

		// Build the program executable
		//
		err = clBuildProgram(program , 0, NULL, NULL, NULL, NULL);
		if(err != CL_SUCCESS) {
			char *errors[16384] = {0};
			err = clGetProgramBuildInfo(program, OpenCL::device_id, CL_PROGRAM_BUILD_LOG, sizeof(errors),
									    errors, NULL);
			PanicAlert("Error log:\n%s\n", errors);
			return NULL;
		}


		return program;
}
cl_kernel CompileKernel(cl_program program, const char *Function)
{
		int err;
		// Create the compute kernel in the program we wish to run
		//
		cl_kernel kernel = clCreateKernel(program, Function, &err);
		if (!kernel || err != CL_SUCCESS)
		{
			PanicAlert("Error: Failed to create compute kernel!");
			return NULL;
		} 
		return kernel;
}
#endif
void Destroy() {

#if defined(HAVE_OPENCL) && HAVE_OPENCL
	if(!g_context)
		return;
	clReleaseCommandQueue(g_cmdq); 
	clReleaseContext(g_context);
#endif
}


};
