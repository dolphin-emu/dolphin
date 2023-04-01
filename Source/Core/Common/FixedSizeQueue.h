// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.

// Not fully featured, no safety checking yet. Add features as needed.

// TODO: "inline" storage?

template <class T, int N>
class FixedSizeQueue
{
	T* storage;
	int head;
	int tail;
	int count;  // sacrifice 4 bytes for a simpler implementation. may optimize away in the future.

	// Make copy constructor private for now.
	FixedSizeQueue(FixedSizeQueue& other) {}

public:
	FixedSizeQueue()
	{
		storage = new T[N];
		clear();
	}

	~FixedSizeQueue()
	{
		delete[] storage;
	}

	void clear()
	{
		head = 0;
		tail = 0;
		count = 0;
	}

	void push(T t)
	{
		storage[tail] = t;
		tail++;
		if (tail == N)
			tail = 0;
		count++;
	}

	void pop()
	{
		head++;
		if (head == N)
			head = 0;
		count--;
	}

	T pop_front()
	{
		const T& temp = storage[head];
		pop();
		return temp;
	}

	T& front() { return storage[head]; }
	const T& front() const { return storage[head]; }

	size_t size() const
	{
		return count;
	}
};
