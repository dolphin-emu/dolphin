// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __OPENCL_H__
#define __OPENCL_H__

#include "Common.h"

#ifdef __APPLE__
#define AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER WEAK_IMPORT_ATTRIBUTE
#include <OpenCL/cl.h>
#else
// The CLRun library provides the headers and all the imports.
#include <CL/cl.h>
#include <clrun.h>
#endif

namespace OpenCL
{

extern cl_device_id device_id;
extern cl_context g_context;
extern cl_command_queue g_cmdq;

bool Initialize();

cl_context GetContext();

cl_command_queue GetCommandQueue();

void Destroy();

cl_program CompileProgram(const char *Kernel);
cl_kernel CompileKernel(cl_program program, const char *Function);

void HandleCLError(cl_int error, const char* str = 0);
}

#endif
