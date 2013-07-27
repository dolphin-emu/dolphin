// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef STREAMBUFFER_H
#define STREAMBUFFER_H

#include "VideoCommon.h"
#include "FramebufferManager.h"
#include "GLUtil.h"

// glew < 1.8 doesn't support pinned memory
#ifndef GLEW_AMD_pinned_memory
#define GLEW_AMD_pinned_memory glewIsSupported("GL_AMD_pinned_memory")
#define GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD 0x9160
#endif

namespace OGL
{
enum StreamType {
	DETECT_MASK = 0x7F,
	STREAM_DETECT = (1 << 0),
	MAP_AND_ORPHAN = (1 << 1),
	MAP_AND_SYNC = (1 << 2),
	MAP_AND_RISK = (1 << 3),
	PINNED_MEMORY = (1 << 4),
	BUFFERSUBDATA = (1 << 5),
	BUFFERDATA = (1 << 6)
};

class StreamBuffer {

public:
	StreamBuffer(u32 type, size_t size, StreamType uploadType = DETECT_MASK);
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
	size_t m_used_iterator;
	size_t m_free_iterator;
	GLsync *fences;
};

}

#endif // STREAMBUFFER_H
