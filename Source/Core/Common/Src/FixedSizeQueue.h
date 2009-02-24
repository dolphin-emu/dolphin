// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _FIXED_SIZE_QUEUE_H
#define _FIXED_SIZE_QUEUE_H

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.

// Not fully featured, no safety checking yet. Add features as needed.

// TODO: "inline" storage?

template <class T, int N>
class FixedSizeQueue
{
	T *storage;
	int head;
	int tail;
	int count;  // sacrifice 4 bytes for a simpler implementation. may optimize away in the future.

	// Make copy constructor private for now.
	FixedSizeQueue(FixedSizeQueue &other) {	}

public:
	FixedSizeQueue()
	{
		storage = new T[N];
		clear();
	}

	~FixedSizeQueue()
	{
		delete [] storage;
	}

	void clear() {
		head = 0;
		tail = 0;
		count = 0;
	}

	void push(T t) {
		storage[tail] = t;
		tail++;
		if (tail == N)
			tail = 0;
		count++;
	}

	void pop() {
		head++;
		if (head == N)
			head = 0;
		count--;
	}

	T pop_front() {
		const T &temp = storage[head];
		pop();
		return temp;
	}

	T &front() { return storage[head]; }
	const T &front() const { return storage[head]; }

	size_t size() const {
		return count;
	}
};

#endif  // _FIXED_SIZE_QUEUE_H

