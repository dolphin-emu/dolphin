// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

class StreamBuffer {

public:
	static StreamBuffer* Create(u32 type, size_t size);
	virtual ~StreamBuffer();

	/* This mapping function will return a pair of:
	 * - the pointer to the mapped buffer
	 * - the offset into the real gpu buffer (always multiple of stride)
	 * On mapping, the maximum of size for allocation has to be set.
	 * The size really pushed into this fifo only has to be known on Unmapping.
	 * Mapping invalidates the current buffer content,
	 * so it isn't allowed to access the old content any more.
	 */
	virtual std::pair<u8*, size_t> Map(size_t size, u32 stride = 0) = 0;
	virtual void Unmap(size_t used_size) = 0;

	const u32 m_buffer;

protected:
	StreamBuffer(u32 type, size_t size);
	void CreateFences();
	void DeleteFences();
	void AllocMemory(size_t size);
	void Align(u32 stride);

	const u32 m_buffertype;
	const size_t m_size;

	size_t m_iterator;
	size_t m_used_iterator;
	size_t m_free_iterator;

private:
	GLsync *fences;
};

}
