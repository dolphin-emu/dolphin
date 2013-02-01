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

namespace OGL
{

StreamBuffer::StreamBuffer(u32 type, size_t size)
: m_buffertype(type), m_size(size), m_iterator(0)
{
	glGenBuffers(1, &m_buffer);
	
	m_uploadtype = MAP_AND_ORPHAN;
	
	Init();
}

StreamBuffer::~StreamBuffer()
{
	Shutdown();
	glDeleteBuffers(1, &m_buffer);
}

void StreamBuffer::Align ( u32 stride )
{
	if(m_iterator) {
		m_iterator--;
		m_iterator = m_iterator - (m_iterator % stride) + stride;
	}
}

void StreamBuffer::Alloc ( size_t size )
{
	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
		if(m_iterator+size >= m_size) {
			glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
			m_iterator = 0;
		}
		break;
	case BUFFERSUBDATA:
		m_iterator = 0;
		break;
	}
}

size_t StreamBuffer::Upload ( u8* data, size_t size )
{
	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
		pointer = (u8*)glMapBufferRange(m_buffertype, m_iterator, size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		if(pointer) {
			memcpy(pointer, data, size);
			glUnmapBuffer(m_buffertype);
		} else {
			ERROR_LOG(VIDEO, "buffer mapping failed");
		}
		break;
	case BUFFERSUBDATA:
		glBufferSubData(m_buffertype, m_iterator, size, data);
		break;
	}
	size_t ret = m_iterator;
	m_iterator += size;
	return ret;
}

void StreamBuffer::Init()
{
	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
	case BUFFERSUBDATA:
		glBindBuffer(m_buffertype, m_buffer);
		glBufferData(m_buffertype, m_size, NULL, GL_STREAM_DRAW);
		break;
	}
}

void StreamBuffer::Shutdown()
{
	switch(m_uploadtype) {
	case MAP_AND_ORPHAN:
	case BUFFERSUBDATA:
		break;
	}
}

}
