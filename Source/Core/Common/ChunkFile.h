// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserializaition (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"

// ewww
#if _LIBCPP_VERSION
#define IsTriviallyCopyable(T) std::is_trivially_copyable<typename std::remove_volatile<T>::type>::value
#elif __GNUC__
#define IsTriviallyCopyable(T) std::has_trivial_copy_constructor<T>::value
#elif _MSC_VER >= 1800
// work around bug
#define IsTriviallyCopyable(T) (std::is_trivially_copyable<T>::value || std::is_pod<T>::value)
#elif defined(_MSC_VER)
#define IsTriviallyCopyable(T) std::has_trivial_copy<T>::value
#else
#error No version of is_trivially_copyable
#endif


template <class T>
struct LinkedListItem : public T
{
	LinkedListItem<T> *next;
};

// Wrapper class
class PointerWrap
{
public:
	enum Mode
	{
		MODE_READ = 1, // load
		MODE_WRITE, // save
		MODE_MEASURE, // calculate size
		MODE_VERIFY, // compare
	};

	u8 **ptr;
	Mode mode;

public:
	PointerWrap(u8 **ptr_, Mode mode_) : ptr(ptr_), mode(mode_) {}

	void SetMode(Mode mode_) { mode = mode_; }
	Mode GetMode() const { return mode; }
	u8** GetPPtr() { return ptr; }

	template <typename K, class V>
	void Do(std::map<K, V>& x)
	{
		u32 count = (u32)x.size();
		Do(count);

		switch (mode)
		{
		case MODE_READ:
			for (x.clear(); count != 0; --count)
			{
				std::pair<K, V> pair;
				Do(pair.first);
				Do(pair.second);
				x.insert(pair);
			}
			break;

		case MODE_WRITE:
		case MODE_MEASURE:
		case MODE_VERIFY:
			for (auto& elem : x)
			{
				Do(elem.first);
				Do(elem.second);
			}
			break;
		}
	}

	template <typename V>
	void Do(std::set<V>& x)
	{
		u32 count = (u32)x.size();
		Do(count);

		switch (mode)
		{
		case MODE_READ:
			for (x.clear(); count != 0; --count)
			{
				V value;
				Do(value);
				x.insert(value);
			}
			break;

		case MODE_WRITE:
		case MODE_MEASURE:
		case MODE_VERIFY:
			for (V& val : x)
			{
				Do(val);
			}
			break;
		}
	}

	template <typename T>
	void Do(std::vector<T>& x)
	{
		DoContainer(x);
	}

	template <typename T>
	void Do(std::list<T>& x)
	{
		DoContainer(x);
	}

	template <typename T>
	void Do(std::deque<T>& x)
	{
		DoContainer(x);
	}

	template <typename T>
	void Do(std::basic_string<T>& x)
	{
		DoContainer(x);
	}

	template <typename T, typename U>
	void Do(std::pair<T, U>& x)
	{
		Do(x.first);
		Do(x.second);
	}

	template <typename T, std::size_t N>
	void DoArray(std::array<T,N>& x)
	{
		DoArray(x.data(), (u32)x.size());
	}

	template <typename T>
	void DoArray(T* x, u32 count)
	{
		static_assert(IsTriviallyCopyable(T), "Only sane for trivially copyable types");
		DoVoid(x, count * sizeof(T));
	}

	void Do(Common::Flag& flag)
	{
		bool s = flag.IsSet();
		Do(s);
		if (mode == MODE_READ)
			flag.Set(s);
	}

	template <typename T>
	void Do(T& x)
	{
		static_assert(IsTriviallyCopyable(T), "Only sane for trivially copyable types");
		// Note:
		// Usually we can just use x = **ptr, etc.  However, this doesn't work
		// for unions containing BitFields (long story, stupid language rules)
		// or arrays.  This will get optimized anyway.
		DoVoid((void*)&x, sizeof(x));
	}

	template <typename T>
	void DoPOD(T& x)
	{
		DoVoid((void*)&x, sizeof(x));
	}

	template <typename T>
	void DoPointer(T*& x, T* const base)
	{
		// pointers can be more than 2^31 apart, but you're using this function wrong if you need that much range
		ptrdiff_t offset = x - base;
		Do(offset);
		if (mode == MODE_READ)
		{
			x = base + offset;
		}
	}

	// Let's pretend std::list doesn't exist!
	template <class T, LinkedListItem<T>* (*TNew)(), void (*TFree)(LinkedListItem<T>*), void (*TDo)(PointerWrap&, T*)>
	void DoLinkedList(LinkedListItem<T>*& list_start, LinkedListItem<T>** list_end=0)
	{
		LinkedListItem<T>* list_cur = list_start;
		LinkedListItem<T>* prev = nullptr;

		while (true)
		{
			u8 shouldExist = (list_cur ? 1 : 0);
			Do(shouldExist);
			if (shouldExist == 1)
			{
				LinkedListItem<T>* cur = list_cur ? list_cur : TNew();
				TDo(*this, (T*)cur);
				if (!list_cur)
				{
					if (mode == MODE_READ)
					{
						cur->next = nullptr;
						list_cur = cur;
						if (prev)
							prev->next = cur;
						else
							list_start = cur;
					}
					else
					{
						TFree(cur);
						continue;
					}
				}
			}
			else
			{
				if (mode == MODE_READ)
				{
					if (prev)
						prev->next = nullptr;
					if (list_end)
						*list_end = prev;
					if (list_cur)
					{
						if (list_start == list_cur)
							list_start = nullptr;
						do
						{
							LinkedListItem<T>* next = list_cur->next;
							TFree(list_cur);
							list_cur = next;
						}
						while (list_cur);
					}
				}
				break;
			}
			prev = list_cur;
			list_cur = list_cur->next;
		}
	}

	void DoMarker(const std::string& prevName, u32 arbitraryNumber = 0x42)
	{
		u32 cookie = arbitraryNumber;
		Do(cookie);

		if (mode == PointerWrap::MODE_READ && cookie != arbitraryNumber)
		{
			PanicAlertT("Error: After \"%s\", found %d (0x%X) instead of save marker %d (0x%X). Aborting savestate load...",
				prevName.c_str(), cookie, cookie, arbitraryNumber, arbitraryNumber);
			mode = PointerWrap::MODE_MEASURE;
		}
	}

private:
	template <typename T>
	void DoContainer(T& x)
	{
		u32 size = (u32)x.size();
		Do(size);
		x.resize(size);

		for (auto& elem : x)
			Do(elem);
	}

	__forceinline
	void DoVoid(void *data, u32 size)
	{
		switch (mode)
		{
		case MODE_READ:
			memcpy(data, *ptr, size);
			break;

		case MODE_WRITE:
			memcpy(*ptr, data, size);
			break;

		case MODE_MEASURE:
			break;

		case MODE_VERIFY:
			_dbg_assert_msg_(COMMON, !memcmp(data, *ptr, size),
				"Savestate verification failure: buf %p != %p (size %u).\n",
					data, *ptr, size);
			break;
		}

		*ptr += size;
	}
};

class CChunkFileReader
{
public:
	// Load file template
	template<class T>
	static bool Load(const std::string& _rFilename, u32 _Revision, T& _class)
	{
		INFO_LOG(COMMON, "ChunkReader: Loading %s" , _rFilename.c_str());

		if (!File::Exists(_rFilename))
			return false;

		// Check file size
		const u64 fileSize = File::GetSize(_rFilename);
		static const u64 headerSize = sizeof(SChunkHeader);
		if (fileSize < headerSize)
		{
			ERROR_LOG(COMMON,"ChunkReader: File too small");
			return false;
		}

		File::IOFile pFile(_rFilename, "rb");
		if (!pFile)
		{
			ERROR_LOG(COMMON,"ChunkReader: Can't open file for reading");
			return false;
		}

		// read the header
		SChunkHeader header;
		if (!pFile.ReadArray(&header, 1))
		{
			ERROR_LOG(COMMON,"ChunkReader: Bad header size");
			return false;
		}

		// Check revision
		if (header.Revision != _Revision)
		{
			ERROR_LOG(COMMON,"ChunkReader: Wrong file revision, got %d expected %d",
				header.Revision, _Revision);
			return false;
		}

		// get size
		const u32 sz = (u32)(fileSize - headerSize);
		if (header.ExpectedSize != sz)
		{
			ERROR_LOG(COMMON,"ChunkReader: Bad file size, got %d expected %d",
				sz, header.ExpectedSize);
			return false;
		}

		// read the state
		std::vector<u8> buffer(sz);
		if (!pFile.ReadArray(&buffer[0], sz))
		{
			ERROR_LOG(COMMON,"ChunkReader: Error reading file");
			return false;
		}

		u8* ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_READ);
		_class.DoState(p);

		INFO_LOG(COMMON, "ChunkReader: Done loading %s" , _rFilename.c_str());
		return true;
	}

	// Save file template
	template<class T>
	static bool Save(const std::string& _rFilename, u32 _Revision, T& _class)
	{
		INFO_LOG(COMMON, "ChunkReader: Writing %s" , _rFilename.c_str());
		File::IOFile pFile(_rFilename, "wb");
		if (!pFile)
		{
			ERROR_LOG(COMMON,"ChunkReader: Error opening file for write");
			return false;
		}

		// Get data
		u8 *ptr = nullptr;
		PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
		_class.DoState(p);
		size_t const sz = (size_t)ptr;
		std::vector<u8> buffer(sz);
		ptr = &buffer[0];
		p.SetMode(PointerWrap::MODE_WRITE);
		_class.DoState(p);

		// Create header
		SChunkHeader header;
		header.Revision = _Revision;
		header.ExpectedSize = (u32)sz;

		// Write to file
		if (!pFile.WriteArray(&header, 1))
		{
			ERROR_LOG(COMMON,"ChunkReader: Failed writing header");
			return false;
		}

		if (!pFile.WriteArray(&buffer[0], sz))
		{
			ERROR_LOG(COMMON,"ChunkReader: Failed writing data");
			return false;
		}

		INFO_LOG(COMMON,"ChunkReader: Done writing %s", _rFilename.c_str());
		return true;
	}

private:
	struct SChunkHeader
	{
		u32 Revision;
		u32 ExpectedSize;
	};
};
