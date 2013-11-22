// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Globals.h"
#include "GLUtil.h"
#include "StreamBuffer.h"
#include "MemoryUtil.h"
#include "Render.h"
#include "DriverDetails.h"
#include "OnScreenDisplay.h"

namespace OGL
{

static const u32 SYNC_POINTS = 16;
static const u32 ALIGN_PINNED_MEMORY = 4096;

StreamBuffer::StreamBuffer(u32 type, size_t size, StreamType uploadType)
: m_uploadtype(uploadType), m_buffertype(type), m_size(size)
{
	glGenBuffers(1, &m_buffer);

	bool nvidia = !strcmp(g_ogl_config.gl_vendor, "NVIDIA Corporation");

	if(m_uploadtype & STREAM_DETECT)
	{
		// TODO: move this to InitBackendInfo
		if(g_ActiveConfig.bHackedBufferUpload && DriverDetails::HasBug(DriverDetails::BUG_BROKENHACKEDBUFFER))
		{
			OSD::AddMessage("Vertex Streaming Hack isn't supported by your GPU.", 10000);
			g_ActiveConfig.bHackedBufferUpload = false;
			g_Config.bHackedBufferUpload = false;
		}

		if(!g_ogl_config.bSupportsGLBaseVertex && (m_uploadtype & BUFFERSUBDATA)
			&& !DriverDetails::HasBug(DriverDetails::BUG_BROKENBUFFERSTREAM))
			m_uploadtype = BUFFERSUBDATA;
		else if(!g_ogl_config.bSupportsGLBaseVertex && (m_uploadtype & BUFFERDATA))
			m_uploadtype = BUFFERDATA;
		else if(g_ogl_config.bSupportsGLSync && g_ActiveConfig.bHackedBufferUpload && (m_uploadtype & MAP_AND_RISK))
			m_uploadtype = MAP_AND_RISK;
		else if(g_ogl_config.bSupportsGLSync && g_ogl_config.bSupportsGLPinnedMemory && !(DriverDetails::HasBug(DriverDetails::BUG_BROKENPINNEDMEMORY) && type == GL_ELEMENT_ARRAY_BUFFER) && (m_uploadtype & PINNED_MEMORY))
			m_uploadtype = PINNED_MEMORY;
		else if(nvidia && (m_uploadtype & BUFFERSUBDATA))
			m_uploadtype = BUFFERSUBDATA;
		else if(g_ogl_config.bSupportsGLSync && (m_uploadtype & MAP_AND_SYNC))
			m_uploadtype = MAP_AND_SYNC;
		else
			m_uploadtype = MAP_AND_ORPHAN;
	}

	Init();
}

StreamBuffer::~StreamBuffer()
{
	Shutdown();
	glDeleteBuffers(1, &m_buffer);
}

#define SLOT(x) ((x)*SYNC_POINTS/m_size)

void StreamBuffer::Alloc ( size_t size, u32 stride )
{
	size_t m_iterator_aligned = m_iterator;
	if(m_iterator_aligned && stride) {
		m_iterator_aligned--;
		m_iterator_aligned = m_iterator_aligned - (m_iterator_aligned % stride) + stride;
	}
	size_t iter_end = m_iterator_aligned + size;

	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
		if(iter_end >= m_size) {
			glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
			m_iterator_aligned = 0;
		}
		break;
	case MAP_AND_SYNC:
	case PINNED_MEMORY:

		// insert waiting slots for used memory
		for(size_t i=SLOT(m_used_iterator); i<SLOT(m_iterator); i++)
		{
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}
		m_used_iterator = m_iterator;

		// wait for new slots to end of buffer
		for (size_t i = SLOT(m_free_iterator) + 1; i <= SLOT(iter_end) && i < SYNC_POINTS; i++)
		{
			glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(fences[i]);
		}
		m_free_iterator = iter_end;

		// if buffer is full
		if(iter_end >= m_size) {

			// insert waiting slots in unused space at the end of the buffer
			for (size_t i = SLOT(m_used_iterator); i < SYNC_POINTS; i++)
			{
				fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			}

			// move to the start
			m_used_iterator = m_iterator_aligned = m_iterator = 0; // offset 0 is always aligned
			iter_end = size;

			// wait for space at the start
			for(u32 i=0; i<=SLOT(iter_end); i++)
			{
				glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				glDeleteSync(fences[i]);
			}
			m_free_iterator = iter_end;
		}

		break;
	case MAP_AND_RISK:
		if(iter_end >= m_size) {
			m_iterator_aligned = 0;
		}
		break;
	case BUFFERSUBDATA:
	case BUFFERDATA:
		m_iterator_aligned = 0;
		break;
	case STREAM_DETECT:
	case DETECT_MASK: // Just to shutup warnings
		break;
	}
	m_iterator = m_iterator_aligned;
}

size_t StreamBuffer::Upload ( u8* data, size_t size )
{
	switch(m_uploadtype) {
	case MAP_AND_SYNC:
	case MAP_AND_ORPHAN:
		pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		if(pointer) {
			memcpy(pointer, data, size);
			glUnmapBuffer(m_buffertype);
		} else {
			ERROR_LOG(VIDEO, "Buffer mapping failed");
		}
		break;
	case PINNED_MEMORY:
	case MAP_AND_RISK:
		if(pointer)
			memcpy(pointer+m_iterator, data, size);
		break;
	case BUFFERSUBDATA:
		glBufferSubData(m_buffertype, m_iterator, size, data);
		break;
	case BUFFERDATA:
		glBufferData(m_buffertype, size, data, GL_STREAM_DRAW);
		break;
	case STREAM_DETECT:
	case DETECT_MASK: // Just to shutup warnings
		break;
	}
	size_t ret = m_iterator;
	m_iterator += size;
	return ret;
}

void StreamBuffer::Init()
{
	m_iterator = 0;
	m_used_iterator = 0;
	m_free_iterator = 0;

	switch(m_uploadtype) {
	case MAP_AND_SYNC:
		fences = new GLsync[SYNC_POINTS];
		for(u32 i=0; i<SYNC_POINTS; i++)
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	case MAP_AND_ORPHAN:
	case BUFFERSUBDATA:
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
		break;
	case PINNED_MEMORY:
		glGetError(); // errors before this allocation should be ignored
		fences = new GLsync[SYNC_POINTS];
		for(u32 i=0; i<SYNC_POINTS; i++)
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		pointer = (u8*)AllocateAlignedMemory(ROUND_UP(m_size,ALIGN_PINNED_MEMORY), ALIGN_PINNED_MEMORY );
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_buffer);
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, ROUND_UP(m_size,ALIGN_PINNED_MEMORY), pointer, GL_STREAM_COPY);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
		glBindBuffer(m_buffertype, m_buffer);

		// on error, switch to another backend. some old catalyst seems to have broken pinned memory support
		if(glGetError() != GL_NO_ERROR) {
			ERROR_LOG(VIDEO, "Pinned memory detected, but not working. Please report this: %s, %s, %s", g_ogl_config.gl_vendor, g_ogl_config.gl_renderer, g_ogl_config.gl_version);
			Shutdown();
			m_uploadtype = MAP_AND_SYNC;
			Init();
		}
		break;
	case MAP_AND_RISK:
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
		pointer = (u8*)glMapBufferRange(m_buffertype, 0, m_size, GL_MAP_WRITE_BIT);
		glUnmapBuffer(m_buffertype);
		if(!pointer)
			ERROR_LOG(VIDEO, "Buffer allocation failed");
		break;
	case BUFFERDATA:
		glBindBuffer(m_buffertype, m_buffer);
		break;
	case STREAM_DETECT:
	case DETECT_MASK: // Just to shutup warnings
		break;
	}
}

void StreamBuffer::Shutdown()
{
	switch(m_uploadtype) {
	case MAP_AND_SYNC:
		DeleteFences();
		break;
	case MAP_AND_RISK:
	case MAP_AND_ORPHAN:
	case BUFFERSUBDATA:
	case BUFFERDATA:
		break;
	case PINNED_MEMORY:
		DeleteFences();
		glBindBuffer(m_buffertype, 0);
		glFinish(); // ogl pipeline must be flushed, else this buffer can be in use
		FreeAlignedMemory(pointer);
		break;
	case STREAM_DETECT:
	case DETECT_MASK: // Just to shutup warnings
		break;
	}
}

void StreamBuffer::DeleteFences()
{
	for (size_t i = SLOT(m_free_iterator) + 1; i < SYNC_POINTS; i++)
	{
		glDeleteSync(fences[i]);
	}
	for (size_t i = 0; i < SLOT(m_iterator); i++)
	{
		glDeleteSync(fences[i]);
	}
	delete [] fences;
}

}
