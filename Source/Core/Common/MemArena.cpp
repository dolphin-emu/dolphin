// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MemArena.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#ifdef ANDROID
#include <sys/ioctl.h>
#include <linux/ashmem.h>
#endif
#endif

#ifdef ANDROID
#define ASHMEM_DEVICE "/dev/ashmem"

static int AshmemCreateFileMapping(const char *name, size_t size)
{
	int fd, ret;
	fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return fd;

	// We don't really care if we can't set the name, it is optional
	ioctl(fd, ASHMEM_SET_NAME, name);

	ret = ioctl(fd, ASHMEM_SET_SIZE, size);
	if (ret < 0)
	{
		close(fd);
		NOTICE_LOG(MEMMAP, "Ashmem returned error: 0x%08x", ret);
		return ret;
	}
	return fd;
}
#endif

void MemArena::GrabLowMemSpace(size_t size)
{
#ifdef _WIN32
	hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)(size), nullptr);
#elif defined(ANDROID)
	fd = AshmemCreateFileMapping("Dolphin-emu", size);
	if (fd < 0)
	{
		NOTICE_LOG(MEMMAP, "Ashmem allocation failed");
		return;
	}
#else
	for (int i = 0; i < 10000; i++)
	{
		std::string file_name = StringFromFormat("dolphinmem.%d", i);
		fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd != -1)
		{
			shm_unlink(file_name.c_str());
			break;
		}
		else if (errno != EEXIST)
		{
			ERROR_LOG(MEMMAP, "shm_open failed: %s", strerror(errno));
			return;
		}
	}
	if (ftruncate(fd, size) < 0)
		ERROR_LOG(MEMMAP, "Failed to allocate low memory space");
#endif
}


void MemArena::ReleaseSpace()
{
#ifdef _WIN32
	CloseHandle(hMemoryMapping);
	hMemoryMapping = 0;
#else
	close(fd);
#endif
}


void *MemArena::CreateView(s64 offset, size_t size, void *base)
{
#ifdef _WIN32
	return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
#else
	void *retval = mmap(
		base, size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | ((base == nullptr) ? 0 : MAP_FIXED),
		fd, offset);

	if (retval == MAP_FAILED)
	{
		NOTICE_LOG(MEMMAP, "mmap failed");
		return nullptr;
	}
	else
	{
		return retval;
	}
#endif
}


void MemArena::ReleaseView(void* view, size_t size)
{
#ifdef _WIN32
	UnmapViewOfFile(view);
#else
	munmap(view, size);
#endif
}


u8* MemArena::Find4GBBase()
{
#if _ARCH_64
#ifdef _WIN32
	// 64 bit
	u8* base = (u8*)VirtualAlloc(0, 0xE1000000, MEM_RESERVE, PAGE_READWRITE);
	VirtualFree(base, 0, MEM_RELEASE);
	return base;
#else
	// Very precarious - mmap cannot return an error when trying to map already used pages.
	// This makes the Windows approach above unusable on Linux, so we will simply pray...
	return reinterpret_cast<u8*>(0x2300000000ULL);
#endif

#else // 32 bit
#ifdef ANDROID
	// Android 4.3 changed how mmap works.
	// if we map it private and then munmap it, we can't use the base returned.
	// This may be due to changes in them support a full SELinux implementation.
	const int flags = MAP_ANON | MAP_SHARED;
#else
	const int flags = MAP_ANON | MAP_PRIVATE;
#endif
	const u32 MemSize = 0x31000000;
	void* base = mmap(0, MemSize, PROT_NONE, flags, -1, 0);
	if (base == MAP_FAILED)
	{
		PanicAlert("Failed to map 1 GB of memory space: %s", strerror(errno));
		return 0;
	}
	munmap(base, MemSize);
	return static_cast<u8*>(base);
#endif
}


// yeah, this could also be done in like two bitwise ops...
#define SKIP(a_flags, b_flags) \
	if (!(a_flags & MV_WII_ONLY) && (b_flags & MV_WII_ONLY)) \
		continue; \
	if (!(a_flags & MV_FAKE_VMEM) && (b_flags & MV_FAKE_VMEM)) \
		continue; \

static bool Memory_TryBase(u8 *base, MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	// OK, we know where to find free space. Now grab it!
	// We just mimic the popular BAT setup.
	u32 shm_position = 0;

	// Zero all the pointers to be sure.
	for (int i = 0; i < num_views; i++)
	{
		views[i].mapped_ptr = nullptr;

		if (!(views[i].flags & MV_MIRROR_PREVIOUS) && i > 0)
			shm_position += views[i - 1].size;

		views[i].shm_position = shm_position;
	}

	int i;
	for (i = 0; i < num_views; i++)
	{
		MemoryView* view = &views[i];
		void* view_base;
		bool use_sw_mirror;

		SKIP(flags, view->flags);

#if _ARCH_64
		// On 64-bit, we map the same file position multiple times, so we
		// don't need the software fallback for the mirrors.
		view_base = base + view->virtual_address;
		use_sw_mirror = false;
#else
		// On 32-bit, we don't have the actual address space to store all
		// the mirrors, so we just map the fallbacks somewhere in our address
		// space and use the software fallbacks for mirroring.
		view_base = base + (view->virtual_address & 0x3FFFFFFF);
		use_sw_mirror = true;
#endif

		if (use_sw_mirror && (view->flags & MV_MIRROR_PREVIOUS))
		{
			view->view_ptr = views[i - 1].view_ptr;
		}
		else
		{
			view->mapped_ptr = arena->CreateView(view->shm_position, view->size, view_base);
			view->view_ptr = view->mapped_ptr;
		}

		if (!view->view_ptr)
			goto bail;

		if (view->out_ptr)
			*(view->out_ptr) = (u8*) view->view_ptr;
	}

	return true;

bail:
	// Argh! ERROR! Free what we grabbed so far so we can try again.
	MemoryMap_Shutdown(views, i+1, flags, arena);
	return false;
}

u8 *MemoryMap_Setup(MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	u32 total_mem = 0;

	for (int i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if ((views[i].flags & MV_MIRROR_PREVIOUS) == 0)
			total_mem += views[i].size;
	}
	// Grab some pagefile backed memory out of the void ...
	arena->GrabLowMemSpace(total_mem);

	// Now, create views in high memory where there's plenty of space.
	u8 *base = MemArena::Find4GBBase();
	// This really shouldn't fail - in 64-bit, there will always be enough
	// address space.
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return nullptr;
	}

	return base;
}

void MemoryMap_Shutdown(MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	std::set<void*> freeset;
	for (int i = 0; i < num_views; i++)
	{
		MemoryView* view = &views[i];
		if (view->mapped_ptr && *(u8*)view->mapped_ptr && !freeset.count(view->mapped_ptr))
		{
			arena->ReleaseView(view->mapped_ptr, view->size);
			freeset.insert(view->mapped_ptr);
			view->mapped_ptr = nullptr;
		}
	}
}
