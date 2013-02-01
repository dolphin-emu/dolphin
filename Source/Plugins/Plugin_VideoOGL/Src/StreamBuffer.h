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


#ifndef STREAMBUFFER_H
#define STREAMBUFFER_H

#include "VideoCommon.h"
#include "FramebufferManager.h"
#include "GLUtil.h"

namespace OGL
{
enum StreamType {
	STREAM_DETECT,
	MAP_AND_ORPHAN,
	MAP_AND_SYNC,
	BUFFERSUBDATA
};

class StreamBuffer {

public:
	StreamBuffer(u32 type, size_t size, StreamType uploadType = STREAM_DETECT);
	~StreamBuffer();
	
	void Alloc(size_t size, u32 stride = 0);
	size_t Upload(u8 *data, size_t size);
	
	u32 getBuffer() { return m_buffer; }
	
private:
	void Init();
	void Shutdown();
	
	StreamType m_uploadtype;
	u32 m_buffer;
	u32 m_buffertype;
	size_t m_size;
	u8 *pointer;
	size_t m_iterator;
	size_t m_last_iterator;
	GLsync *fences;
};

}

#endif // STREAMBUFFER_H
