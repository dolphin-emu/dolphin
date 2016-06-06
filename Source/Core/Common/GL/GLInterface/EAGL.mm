// Copyright 2016 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "Common/GL/GLInterface/EAGL.h"
#include "Common/Logging/Log.h"
#include "Common/GL/GLUtil.h"

void cInterfaceEAGL::Swap()
{
	[m_context presentRenderbuffer:GL_RENDERBUFFER];
}

GLInterfaceMode cInterfaceEAGL::GetMode()
{
	return GLInterfaceMode::MODE_OPENGLES3;
}

void cInterfaceEAGL::DetectMode()
{
	s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
}

bool cInterfaceEAGL::Create(void *window_handle, bool core)
{
	m_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
	[EAGLContext setCurrentContext: m_context];
	m_glkView = (GLKView *)window_handle;
	m_glkView.context = m_context;
	int screenScale = [UIScreen mainScreen].scale;
	s_backbuffer_width = m_glkView.frame.size.width * screenScale;
	s_backbuffer_height = m_glkView.frame.size.height * screenScale;
	return true;
}

void cInterfaceEAGL::Prepare()
{
	[m_glkView bindDrawable];
	if (!m_bufferStorageCreated)
	{
		if (![m_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)m_glkView.layer])
		{
			NSLog(@"Unable to create renderbufferstorage");
		}
		m_bufferStorageCreated = true;
	}
}

int cInterfaceEAGL::GetDefaultFramebuffer()
{
	GLint defaultFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
	return defaultFBO;
}


bool cInterfaceEAGL::Create(cInterfaceBase* main_context)
{
	NSLog(@"Create(cInterfaceBase* main_context)");

	return true;
}

bool cInterfaceEAGL::MakeCurrent()
{
	[EAGLContext setCurrentContext: m_context];
	return true;
}

void cInterfaceEAGL::UpdateHandle(void* window_handle)
{
	m_glkView = (GLKView *)window_handle;
}

void cInterfaceEAGL::UpdateSurface()
{
	
}

bool cInterfaceEAGL::ClearCurrent()
{
	[EAGLContext setCurrentContext:nil];
	return true;
}

// Close backend
void cInterfaceEAGL::Shutdown()
{
	
}
