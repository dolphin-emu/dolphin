// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <array>
#include <cassert>

#include "Atomic.h"

// Implementation of a lockless single reader, single writer ring buffer.
//
// This should only be used to store small POD. If you need to store larger
// data, consider using FifoQueue instead.

namespace Common
{

// T is the element type contained in the RingBuffer.
// N is the size of the RingBuffer, i.e. the number of elements that can be
// written without reading. If you write more, weird things will happen.
template <typename T, unsigned int N>
class RingBuffer
{
public:
	RingBuffer() : m_readIndex(0), m_writeIndex(0) {}

	// Get elements from the buffer.
	//
	// Should be called only from the reader thread.
	T Pop()
	{
		T ret;
		PopMultiple(&ret, 1);
		return ret;
	}
	void PopMultiple(T* out, u32 count)
	{
		assert(count < N);

		u32 r = Common::AtomicLoad(m_readIndex);
		if (r + count < N)
		{
			memcpy(out, &m_buffer[r], count * sizeof (T));
			r += count;
		}
		else
		{
			u32 count_before_end = N - r;
			memcpy(out, &m_buffer[r], count_before_end * sizeof (T));
			memcpy(out + count_before_end, &m_buffer[0],
			       (count - count_before_end) * sizeof (T));
			r = (r + count) % N;
		}
		Common::AtomicStore(m_readIndex, r);
	}

	// Push elements to the buffer.
	//
	// Should be called only from the writer thread.
	void Push(T elem)
	{
		PushMultiple(&elem, 1);
	}
	void PushMultiple(const T* elem, u32 count)
	{
		assert(count < N);

		u32 w = Common::AtomicLoad(m_writeIndex);
		if (w + count < N)
		{
			memcpy(&m_buffer[w], elem, count * sizeof (T));
			w += count;
		}
		else
		{
			u32 count_before_end = N - w;
			memcpy(&m_buffer[w], elem, count_before_end * sizeof (T));
			memcpy(&m_buffer[0], elem + count_before_end,
			       (count - count_before_end) * sizeof (T));
			w = (w + count) % N;
		}
		Common::AtomicStore(m_writeIndex, w);
	}

	// Returns a lower bound on the number of elements available to read.
	//
	// Should be called only from the reader thread.
	u32 ReadableCount()
	{
		u32 r = Common::AtomicLoad(m_readIndex);
		u32 w = Common::AtomicLoad(m_writeIndex);

		return (N + w - r) % N;
	}

	// Returns a lower bound on the number of elements that can be pushed
	// without overwriting unread data or filling the buffer (r=w).
	//
	// Should be called only from the writer thread.
	u32 WritableCount()
	{
		return N - ReadableCount() - 1;
	}

private:
	std::array<T, N> m_buffer;
	volatile u32 m_readIndex;
	volatile u32 m_writeIndex;
};

}

#endif
