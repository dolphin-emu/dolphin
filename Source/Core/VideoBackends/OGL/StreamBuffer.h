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
	virtual std::pair<u8*, size_t> Map(size_t size) = 0;
	virtual void Unmap(size_t used_size) = 0;

	inline std::pair<u8*, size_t> Map(size_t size, u32 stride)
	{
		u32 padding = m_iterator % stride;
		if (padding)
		{
			m_iterator += stride - padding;
		}
		return Map(size);
        }

	const u32 m_buffer;

protected:
	StreamBuffer(u32 type, size_t size);
	void CreateFences();
	void DeleteFences();
	void AllocMemory(size_t size);

	const u32 m_buffertype;
	const size_t m_size;

	size_t m_iterator;
	size_t m_used_iterator;
	size_t m_free_iterator;

private:
	static const int SYNC_POINTS = 16;
	inline int SLOT(size_t x) const { return x >> m_bit_per_slot; }
	const int m_bit_per_slot;

	GLsync fences[SYNC_POINTS];
};

}
