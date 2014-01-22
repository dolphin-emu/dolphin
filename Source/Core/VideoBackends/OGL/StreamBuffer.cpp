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

StreamBuffer::StreamBuffer(u32 type, size_t size)
: m_buffertype(type), m_size(size)
{
	glGenBuffers(1, &m_buffer);

	bool nvidia = !strcmp(g_ogl_config.gl_vendor, "NVIDIA Corporation");

	if (g_ogl_config.bSupportsGLBufferStorage &&
		!(DriverDetails::HasBug(DriverDetails::BUG_BROKENBUFFERSTORAGE) && type == GL_ARRAY_BUFFER))
		m_uploadtype = BUFFERSTORAGE;
	else if(!g_ogl_config.bSupportsGLBaseVertex && !DriverDetails::HasBug(DriverDetails::BUG_BROKENBUFFERSTREAM))
		m_uploadtype = BUFFERSUBDATA;
	else if(!g_ogl_config.bSupportsGLBaseVertex)
		m_uploadtype = BUFFERDATA;
	else if(g_ogl_config.bSupportsGLSync && g_ogl_config.bSupportsGLPinnedMemory &&
		!(DriverDetails::HasBug(DriverDetails::BUG_BROKENPINNEDMEMORY) && type == GL_ELEMENT_ARRAY_BUFFER))
		m_uploadtype = PINNED_MEMORY;
	else if(nvidia)
		m_uploadtype = BUFFERSUBDATA;
	else if(g_ogl_config.bSupportsGLSync)
		m_uploadtype = MAP_AND_SYNC;
	else
		m_uploadtype = MAP_AND_ORPHAN;

	Init();
}

StreamBuffer::~StreamBuffer()
{
	Shutdown();
	glDeleteBuffers(1, &m_buffer);
}

#define SLOT(x) ((x)*SYNC_POINTS/m_size)

u8* StreamBuffer::Map ( size_t size, u32 stride )
{
	if(m_iterator && stride) {
		m_iterator--;
		m_iterator = m_iterator - (m_iterator % stride) + stride;
	}

	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
		if(m_iterator + size >= m_size) {
			glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
			m_iterator = 0;
		}
		break;
	case MAP_AND_SYNC:
	case PINNED_MEMORY:
	case BUFFERSTORAGE:
		// insert waiting slots for used memory
		for (size_t i = SLOT(m_used_iterator); i < SLOT(m_iterator); i++)
		{
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}
		m_used_iterator = m_iterator;

		// wait for new slots to end of buffer
		for (size_t i = SLOT(m_free_iterator) + 1; i <= SLOT(m_iterator + size) && i < SYNC_POINTS; i++)
		{
			glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(fences[i]);
		}
		m_free_iterator = m_iterator + size;

		// if buffer is full
		if (m_iterator + size >= m_size) {

			// insert waiting slots in unused space at the end of the buffer
			for (size_t i = SLOT(m_used_iterator); i < SYNC_POINTS; i++)
			{
				fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			}

			// move to the start
			m_used_iterator = m_iterator = 0; // offset 0 is always aligned

			// wait for space at the start
			for (u32 i = 0; i <= SLOT(m_iterator + size); i++)
			{
				glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				glDeleteSync(fences[i]);
			}
			m_free_iterator = m_iterator + size;
		}
		break;
	case BUFFERSUBDATA:
	case BUFFERDATA:
		m_iterator = 0;
		break;
	}

	// MAP_AND_* methods need to remap this buffer every time
	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
	case MAP_AND_SYNC:
		pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT) - m_iterator;
		break;
	case PINNED_MEMORY:
	case BUFFERSTORAGE:
	case BUFFERSUBDATA:
	case BUFFERDATA:
		break;
	}
	return pointer + m_iterator;
}

size_t StreamBuffer::Unmap(size_t used_size)
{
	size_t ret = m_iterator;
	switch(m_uploadtype) {
	case MAP_AND_SYNC:
	case MAP_AND_ORPHAN:
		glFlushMappedBufferRange(m_buffertype, 0, used_size);
		glUnmapBuffer(m_buffertype);
		break;
	case PINNED_MEMORY:
	case BUFFERSTORAGE:
	case BUFFERSUBDATA:
		glBufferSubData(m_buffertype, 0, used_size, pointer);
		break;
	case BUFFERDATA:
		glBufferData(m_buffertype, used_size, pointer, GL_STREAM_DRAW);
		break;
	}
	m_iterator += used_size;
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
		pointer = new u8[m_size];
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

	case BUFFERSTORAGE:
		glGetError(); // errors before this allocation should be ignored
		fences = new GLsync[SYNC_POINTS];
		for (u32 i = 0; i<SYNC_POINTS; i++)
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		glBindBuffer(m_buffertype, m_buffer);

		// PERSISTANT_BIT to make sure that the buffer can be used while mapped
		// COHERENT_BIT is set so we don't have to use a MemoryBarrier on write
		// CLIENT_STORAGE_BIT is set since we access the buffer more frequently on the client side then server side
		glBufferStorage(m_buffertype, m_size, NULL, 
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT); 
		pointer = (u8*)glMapBufferRange(m_buffertype, 0, m_size, 
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT); 
		if(!pointer)
			ERROR_LOG(VIDEO, "Buffer allocation failed");
		break;

	case BUFFERDATA:
		glBindBuffer(m_buffertype, m_buffer);
		pointer = new u8[m_size];
		break;
	}
}

void StreamBuffer::Shutdown()
{
	switch(m_uploadtype) {
	case MAP_AND_SYNC:
		DeleteFences();
		break;
	case MAP_AND_ORPHAN:
		break;
	case BUFFERSUBDATA:
	case BUFFERDATA:
		delete [] pointer;
		break;
	case PINNED_MEMORY:
		DeleteFences();
		glBindBuffer(m_buffertype, 0);
		glFinish(); // ogl pipeline must be flushed, else this buffer can be in use
		FreeAlignedMemory(pointer);
		break;
	case BUFFERSTORAGE:
		DeleteFences();
		glUnmapBuffer(m_buffertype);
		glBindBuffer(m_buffertype, 0);
		glFinish(); // ogl pipeline must be flushed, else this buffer can be in use
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
