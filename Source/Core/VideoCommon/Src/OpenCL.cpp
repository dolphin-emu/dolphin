// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO: Make a more centralized version of this (for now every backend that will use it will create its own context, which is weird). An object maybe?

#include "OpenCL.h"
#include "Common.h"
#include "Timer.h"

namespace OpenCL
{

cl_device_id device_id = NULL;
cl_context g_context = NULL;
cl_command_queue g_cmdq = NULL;

bool g_bInitialized = false;

bool Initialize()
{
	if(g_bInitialized)
		return true;

	if(g_context)
		return false;
	int err;			// error code returned from api calls

#ifdef __APPLE__
	// If OpenCL is weakly linked and not found, its symbols will be NULL
	if (clGetPlatformIDs == NULL)
		return false;
#else
	clrInit();
	if(!clrHasOpenCL())
		return false;
#endif

	// Connect to a compute device
	cl_uint numPlatforms;
	cl_platform_id platform = NULL;
	err = clGetPlatformIDs(0, NULL, &numPlatforms);

	if (err != CL_SUCCESS)
	{
		HandleCLError(err, "clGetPlatformIDs failed.");
		return false;
	}

	if (0 < numPlatforms)
	{
		cl_platform_id* platforms = new cl_platform_id[numPlatforms];
		err = clGetPlatformIDs(numPlatforms, platforms, NULL);

		if (err != CL_SUCCESS)
		{
			HandleCLError(err, "clGetPlatformIDs failed.");
			return false;
		}

		char pbuf[100];
		err = clGetPlatformInfo(platforms[0], CL_PLATFORM_VENDOR, sizeof(pbuf),
			pbuf, NULL);

		if (err != CL_SUCCESS)
		{
			HandleCLError(err, "clGetPlatformInfo failed.");
			return false;
		}

		platform = platforms[0];
		delete[] platforms;
	}
	else
	{
		PanicAlert("No OpenCL platform found.");
		return false;
	}

	cl_context_properties cps[3] = {CL_CONTEXT_PLATFORM,
		(cl_context_properties)platform, 0};

	cl_context_properties* cprops = (NULL == platform) ? NULL : cps;

	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, NULL);
	if (err != CL_SUCCESS)
	{
		HandleCLError(err, "Failed to create a device group!");
		return false;
	}

	// Create a compute context
	g_context = clCreateContext(cprops, 1, &device_id, NULL, NULL, &err);
	if (!g_context)
	{
		HandleCLError(err, "Failed to create a compute context!");
		return false;
	}

	// Create a command commands
	g_cmdq = clCreateCommandQueue(g_context, device_id, 0, &err);
	if (!g_cmdq)
	{
		HandleCLError(err, "Failed to create a command commands!");
		return false;
	}

	g_bInitialized = true;
	return true;
}

cl_context GetContext()
{
	return g_context;
}

cl_command_queue GetCommandQueue()
{
	return g_cmdq;
}

cl_program CompileProgram(const char *Kernel)
{
	u32 compileStart = Common::Timer::GetTimeMs();
	cl_int err;
	cl_program program;
	program = clCreateProgramWithSource(OpenCL::g_context, 1,
		(const char **) & Kernel, NULL, &err);

	if (!program)
	{
		HandleCLError(err, "Error: Failed to create compute program!");
	}

	// Build the program executable
	err = clBuildProgram(program , 0, NULL, NULL, NULL, NULL);
	if(err != CL_SUCCESS) {
		HandleCLError(err, "Error: failed to build program");

		char *buildlog = NULL;
		size_t buildlog_size = 0;

		clGetProgramBuildInfo(program, OpenCL::device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildlog_size);
		buildlog = new char[buildlog_size + 1];
		err = clGetProgramBuildInfo(program, OpenCL::device_id, CL_PROGRAM_BUILD_LOG, buildlog_size, buildlog, NULL);
		buildlog[buildlog_size] = 0;
		
		if(err != CL_SUCCESS)
		{
			HandleCLError(err, "Error: can't get build log");
		} else
		{
			ERROR_LOG(COMMON, "Error log:\n%s\n", buildlog);
		}

		delete[] buildlog;
		return NULL;
	}

	INFO_LOG(COMMON, "OpenCL CompileProgram took %.3f seconds",
		(float)(Common::Timer::GetTimeMs() - compileStart) / 1000.0);
	return program;
}

cl_kernel CompileKernel(cl_program program, const char *Function)
{
	u32 compileStart = Common::Timer::GetTimeMs();
	int err;

	// Create the compute kernel in the program we wish to run
	cl_kernel kernel = clCreateKernel(program, Function, &err);
	if (!kernel || err != CL_SUCCESS)
	{
		char buffer[1024];
		sprintf(buffer, "Failed to create compute kernel '%s' !", Function);
		HandleCLError(err, buffer);
		return NULL;
	}
	INFO_LOG(COMMON, "OpenCL CompileKernel took %.3f seconds",
		(float)(Common::Timer::GetTimeMs() - compileStart) / 1000.0);
	return kernel;
}

void Destroy()
{
	if (g_cmdq)
	{
		clReleaseCommandQueue(g_cmdq);
		g_cmdq = NULL;
	}
	if (g_context)
	{
		clReleaseContext(g_context);
		g_context = NULL;
	}		
	g_bInitialized = false;
}

void HandleCLError(cl_int error, const char* str)
{
	const char* name;
	switch(error)
	{
#define CL_ERROR(x) case (x): name = #x; break
		CL_ERROR(CL_SUCCESS);
		CL_ERROR(CL_DEVICE_NOT_FOUND);
		CL_ERROR(CL_DEVICE_NOT_AVAILABLE);
		CL_ERROR(CL_COMPILER_NOT_AVAILABLE);
		CL_ERROR(CL_MEM_OBJECT_ALLOCATION_FAILURE);
		CL_ERROR(CL_OUT_OF_RESOURCES);
		CL_ERROR(CL_OUT_OF_HOST_MEMORY);
		CL_ERROR(CL_PROFILING_INFO_NOT_AVAILABLE);
		CL_ERROR(CL_MEM_COPY_OVERLAP);
		CL_ERROR(CL_IMAGE_FORMAT_MISMATCH);
		CL_ERROR(CL_IMAGE_FORMAT_NOT_SUPPORTED);
		CL_ERROR(CL_BUILD_PROGRAM_FAILURE);
		CL_ERROR(CL_MAP_FAILURE);
		CL_ERROR(CL_INVALID_VALUE);
		CL_ERROR(CL_INVALID_DEVICE_TYPE);
		CL_ERROR(CL_INVALID_PLATFORM);
		CL_ERROR(CL_INVALID_DEVICE);
		CL_ERROR(CL_INVALID_CONTEXT);
		CL_ERROR(CL_INVALID_QUEUE_PROPERTIES);
		CL_ERROR(CL_INVALID_COMMAND_QUEUE);
		CL_ERROR(CL_INVALID_HOST_PTR);
		CL_ERROR(CL_INVALID_MEM_OBJECT);
		CL_ERROR(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
		CL_ERROR(CL_INVALID_IMAGE_SIZE);
		CL_ERROR(CL_INVALID_SAMPLER);
		CL_ERROR(CL_INVALID_BINARY);
		CL_ERROR(CL_INVALID_BUILD_OPTIONS);
		CL_ERROR(CL_INVALID_PROGRAM);
		CL_ERROR(CL_INVALID_PROGRAM_EXECUTABLE);
		CL_ERROR(CL_INVALID_KERNEL_NAME);
		CL_ERROR(CL_INVALID_KERNEL_DEFINITION);
		CL_ERROR(CL_INVALID_KERNEL);
		CL_ERROR(CL_INVALID_ARG_INDEX);
		CL_ERROR(CL_INVALID_ARG_VALUE);
		CL_ERROR(CL_INVALID_ARG_SIZE);
		CL_ERROR(CL_INVALID_KERNEL_ARGS);
		CL_ERROR(CL_INVALID_WORK_DIMENSION);
		CL_ERROR(CL_INVALID_WORK_GROUP_SIZE);
		CL_ERROR(CL_INVALID_WORK_ITEM_SIZE);
		CL_ERROR(CL_INVALID_GLOBAL_OFFSET);
		CL_ERROR(CL_INVALID_EVENT_WAIT_LIST);
		CL_ERROR(CL_INVALID_EVENT);
		CL_ERROR(CL_INVALID_OPERATION);
		CL_ERROR(CL_INVALID_GL_OBJECT);
		CL_ERROR(CL_INVALID_BUFFER_SIZE);
		CL_ERROR(CL_INVALID_MIP_LEVEL);
#undef CL_ERROR
	default:
		name = "Unknown error code";
	}
	if(!str)
		str = "";
	ERROR_LOG(COMMON, "OpenCL error: %s %s (%d)", str, name, error);
	}
}
