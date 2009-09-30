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

cl_context g_context = NULL;
cl_command_queue g_cmdq = NULL;

bool Initialize() {
	if(g_context)
		return false;
	
#if defined(HAVE_OPENCL) && HAVE_OPENCL

	int err;                            // error code returned from api calls
	cl_device_id device_id;
            
    // Connect to a compute device
    //
    int gpu = 1; // I think we should use CL_DEVICE_TYPE_ALL
    err = clGetDeviceIDs(NULL, gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
    if (err != CL_SUCCESS)
    {
        PanicAlert("Error: Failed to create a device group!\n");
        return false;
    }
  
    // Create a compute context 
    //
    g_context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!g_context)
    {
        PanicAlert("Error: Failed to create a compute context!\n");
        return false;
    }

    // Create a command commands
    //
    g_cmdq = clCreateCommandQueue(g_context, device_id, 0, &err);
    if (!g_cmdq)
    {
        PanicAlert("Error: Failed to create a command commands!\n");
        return false;
    }

	return true;
#else
	return false;
#endif
}

cl_context GetInstance() {
	return g_context;
}

cl_command_queue GetCommandQueue() {
	return g_cmdq;
}

cl_program CompileProgram(const char *program, unsigned int size) {
	// TODO
	return NULL;
}

void Destroy() {
	if(!g_context)
		return;

#if defined(HAVE_OPENCL) && HAVE_OPENCL
	clReleaseCommandQueue(g_cmdq); 
	clReleaseContext(g_context);
#endif
}


};