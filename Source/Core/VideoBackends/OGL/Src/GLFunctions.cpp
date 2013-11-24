// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DriverDetails.h"
#include "GLFunctions.h"
#include "Log.h"
#include <dlfcn.h>

#ifdef USE_GLES3
PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
PFNGLUNMAPBUFFERPROC glUnmapBuffer;
PFNGLBINDBUFFERRANGEPROC glBindBufferRange;

PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
PFNGLDELETESYNCPROC glDeleteSync;
PFNGLFENCESYNCPROC glFenceSync;

PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
PFNGLBINDSAMPLERPROC glBindSampler;
PFNGLDELETESAMPLERSPROC glDeleteSamplers;
PFNGLGENSAMPLERSPROC glGenSamplers;

PFNGLGETPROGRAMBINARYPROC glGetProgramBinary;
PFNGLPROGRAMBINARYPROC glProgramBinary;
PFNGLPROGRAMPARAMETERIPROC glProgramParameteri;

PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;

PFNGLRENDERBUFFERSTORAGEMULTISAMPLE glRenderbufferStorageMultisample;

PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;

PFNGLBEGINQUERYPROC glBeginQuery;
PFNGLENDQUERYPROC glEndQuery;
PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
PFNGLDELETEQUERIESPROC glDeleteQueries;
PFNGLGENQUERIESPROC glGenQueries;
#endif
namespace GLFunc
{
	void *self;
	void LoadFunction(const char *name, void **func)
	{
#ifdef USE_GLES3
		*func = (void*)eglGetProcAddress(name);
		if (*func == NULL)
		{
			// Fall back to trying dlsym
			if (self) // Just in case dlopen fails
				*func = dlsym(self, name);
			if (*func == NULL)
			{
				ERROR_LOG(VIDEO, "Couldn't load function %s", name);
				exit(0);
			}
		}
#endif
	}

	void Init()
	{
		self = dlopen(NULL, RTLD_LAZY);

		LoadFunction("glUnmapBuffer", (void**)&glUnmapBuffer);
		LoadFunction("glBeginQuery", (void**)&glBeginQuery);
		LoadFunction("glEndQuery", (void**)&glEndQuery);
		LoadFunction("glGetQueryObjectuiv", (void**)&glGetQueryObjectuiv);
		LoadFunction("glDeleteQueries", (void**)&glDeleteQueries);
		LoadFunction("glGenQueries", (void**)&glGenQueries);

		LoadFunction("glMapBufferRange", (void**)&glMapBufferRange);
		LoadFunction("glBindBufferRange", (void**)&glBindBufferRange);
		LoadFunction("glBlitFramebuffer", (void**)&glBlitFramebuffer);

		LoadFunction("glGenVertexArrays", (void**)&glGenVertexArrays);
		LoadFunction("glDeleteVertexArrays", (void**)&glDeleteVertexArrays);
		LoadFunction("glBindVertexArray", (void**)&glBindVertexArray);

		LoadFunction("glClientWaitSync", (void**)&glClientWaitSync);
		LoadFunction("glDeleteSync", (void**)&glDeleteSync);
		LoadFunction("glFenceSync", (void**)&glFenceSync);

		LoadFunction("glSamplerParameterf", (void**)&glSamplerParameterf);
		LoadFunction("glSamplerParameteri", (void**)&glSamplerParameteri);
		LoadFunction("glSamplerParameterfv", (void**)&glSamplerParameterfv);
		LoadFunction("glBindSampler", (void**)&glBindSampler);
		LoadFunction("glDeleteSamplers", (void**)&glDeleteSamplers);
		LoadFunction("glGenSamplers", (void**)&glGenSamplers);

		LoadFunction("glGetProgramBinary", (void**)&glGetProgramBinary);
		LoadFunction("glProgramBinary", (void**)&glProgramBinary);
		LoadFunction("glProgramParameteri", (void**)&glProgramParameteri);

		LoadFunction("glDrawRangeElements", (void**)&glDrawRangeElements);

		LoadFunction("glRenderbufferStorageMultisample", (void**)&glRenderbufferStorageMultisample);

		LoadFunction("glGetUniformBlockIndex", (void**)&glGetUniformBlockIndex);
		LoadFunction("glUniformBlockBinding", (void**)&glUniformBlockBinding);

		dlclose(self);
	}
}
