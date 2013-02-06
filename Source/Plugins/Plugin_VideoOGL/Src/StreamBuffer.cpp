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


#include "Globals.h"
#include "GLUtil.h"
#include "StreamBuffer.h"
#include "MemoryUtil.h"

#ifndef GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD
#define GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD 0x9160
#endif

namespace OGL
{

static const u32 SYNC_POINTS = 16;
static const u32 ALIGN_PINNED_MEMORY = 4096;

StreamBuffer::StreamBuffer(u32 type, size_t size, StreamType uploadType)
: m_uploadtype(uploadType), m_buffertype(type), m_size(size)
{
	glGenBuffers(1, &m_buffer);
	
	if(m_uploadtype == STREAM_DETECT)
	{
		if(g_Config.backend_info.bSupportsGLPinnedMemory && g_Config.backend_info.bSupportsGLSync)
			m_uploadtype = PINNED_MEMORY;
		else if(g_Config.backend_info.bSupportsGLSync)
			m_uploadtype = MAP_AND_RISK;
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

#define SLOT(x) (x)*SYNC_POINTS/m_size

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
	case MAP_AND_RISK:
		
		// insert waiting slots for used memory
		for(u32 i=SLOT(m_used_iterator); i<SLOT(m_iterator); i++)
		{
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}
		m_used_iterator = m_iterator;
		
		// wait for new slots to end of buffer
		for(u32 i=SLOT(m_free_iterator)+1; i<=SLOT(iter_end) && i < SYNC_POINTS; i++)
		{
			glClientWaitSync(fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(fences[i]);
		}
		m_free_iterator = iter_end;
		
		// if buffer is full
		if(iter_end >= m_size) {
			
			// insert waiting slots in unused space at the end of the buffer
			for(u32 i=SLOT(m_used_iterator); i < SYNC_POINTS; i++)
				fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			
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
	case BUFFERSUBDATA:
		m_iterator_aligned = 0;
		break;
	case STREAM_DETECT:
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
			ERROR_LOG(VIDEO, "buffer mapping failed");
		}
		break;
	case PINNED_MEMORY:
	case MAP_AND_RISK:
		memcpy(pointer+m_iterator, data, size);
		break;
	case BUFFERSUBDATA:
		glBufferSubData(m_buffertype, m_iterator, size, data);
		break;
	case STREAM_DETECT:
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
		fences = new GLsync[SYNC_POINTS];
		for(u32 i=0; i<SYNC_POINTS; i++)
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		
		pointer = (u8*)AllocateAlignedMemory(ROUND_UP(m_size,ALIGN_PINNED_MEMORY), ALIGN_PINNED_MEMORY );
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_buffer);
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_size, pointer, GL_STREAM_COPY);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
		glBindBuffer(m_buffertype, m_buffer);
		break;
	case MAP_AND_RISK:
		fences = new GLsync[SYNC_POINTS];
		for(u32 i=0; i<SYNC_POINTS; i++)
			fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
		pointer = (u8*)glMapBuffer(m_buffertype, GL_WRITE_ONLY);
		glUnmapBuffer(m_buffertype);
		
	case STREAM_DETECT:
		break;
	}
}

void StreamBuffer::Shutdown()
{
	switch(m_uploadtype) {
	case MAP_AND_SYNC:
	case MAP_AND_RISK:
		for(u32 i=0; i<SYNC_POINTS; i++)
			glDeleteSync(fences[i]);
		delete [] fences;
		break;
		
	case MAP_AND_ORPHAN:
	case BUFFERSUBDATA:
		break;
	case PINNED_MEMORY:
		for(u32 i=0; i<SYNC_POINTS; i++)
			glDeleteSync(fences[i]);
		delete [] fences;
		glBindBuffer(m_buffertype, 0);
		glFinish(); // ogl pipeline must be flushed, else this buffer can be in use
		FreeAlignedMemory(pointer);
		break;
	case STREAM_DETECT:
		break;
	}
}

}
