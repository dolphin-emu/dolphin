// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "Common.h"
#include "MemoryUtil.h"
#include "MemArena.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#ifdef ANDROID
#include <sys/ioctl.h>
#include <linux/ashmem.h>
#endif
#endif
#include <set>

#if defined(__APPLE__)
static const char* ram_temp_file = "/tmp/gc_mem.tmp";
#elif !defined(_WIN32) // non OSX unixes
static const char* ram_temp_file = "/dev/shm/gc_mem.tmp";
#endif
#ifdef ANDROID
#define ASHMEM_DEVICE "/dev/ashmem"

int AshmemCreateFileMapping(const char *name, size_t size)
{
	int fd, ret;
	fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return fd;

	// We don't really care if we can't set the name, it is optional	
	ret = ioctl(fd, ASHMEM_SET_NAME, name);

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
	hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)(size), NULL);
#elif defined(ANDROID)
	fd = AshmemCreateFileMapping("Dolphin-emu", size);
	if (fd < 0)
	{
		NOTICE_LOG(MEMMAP, "Ashmem allocation failed");
		return;
	}
#else
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	fd = open(ram_temp_file, O_RDWR | O_CREAT, mode);
	unlink(ram_temp_file);
	if (ftruncate(fd, size) < 0)
		ERROR_LOG(MEMMAP, "Failed to allocate low memory space");
	return;
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
		NOTICE_LOG(MEMMAP, "mmap on %s failed", ram_temp_file);
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
#ifdef _M_X64
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

#else
	// 32 bit
#ifdef _WIN32
	// The highest thing in any 1GB section of memory space is the locked cache. We only need to fit it.
	u8* base = (u8*)VirtualAlloc(0, 0x31000000, MEM_RESERVE, PAGE_READWRITE);
	if (base) {
		VirtualFree(base, 0, MEM_RELEASE);
	}
	return base;
#else
#ifdef ANDROID
	const u32 MemSize = 0x04000000;
#else
	const u32 MemSize = 0x31000000; 
#endif
	void* base = mmap(0, MemSize, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (base == MAP_FAILED) {
		PanicAlert("Failed to map 1 GB of memory space: %s", strerror(errno));
		return 0;
	}
#ifndef ANDROID
	// Android 4.3 changes how munmap works which causes crashes.
	// Keep the memory space after allocating it...
	munmap(base, MemSize);
#endif
	return static_cast<u8*>(base);
#endif
#endif
}


// yeah, this could also be done in like two bitwise ops...
#define SKIP(a_flags, b_flags) \
	if (!(a_flags & MV_WII_ONLY) && (b_flags & MV_WII_ONLY)) \
		continue; \
	if (!(a_flags & MV_FAKE_VMEM) && (b_flags & MV_FAKE_VMEM)) \
		continue; \


static bool Memory_TryBase(u8 *base, const MemoryView *views, int num_views, u32 flags, MemArena *arena) {
	// OK, we know where to find free space. Now grab it!
	// We just mimic the popular BAT setup.
	u32 position = 0;
	u32 last_position = 0;

	// Zero all the pointers to be sure.
	for (int i = 0; i < num_views; i++)
	{
		if (views[i].out_ptr_low)
			*views[i].out_ptr_low = 0;
		if (views[i].out_ptr)
			*views[i].out_ptr = 0;
	}

	int i;
	for (i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if (views[i].flags & MV_MIRROR_PREVIOUS) {
			position = last_position;
		} else {
			*(views[i].out_ptr_low) = (u8*)arena->CreateView(position, views[i].size);
			if (!*views[i].out_ptr_low)
				goto bail;
		}
#ifdef _M_X64
		*views[i].out_ptr = (u8*)arena->CreateView(
			position, views[i].size, base + views[i].virtual_address);
#else
		if (views[i].flags & MV_MIRROR_PREVIOUS) {
			// No need to create multiple identical views.
			*views[i].out_ptr = *views[i - 1].out_ptr;
		} else {
			*views[i].out_ptr = (u8*)arena->CreateView(
				position, views[i].size, base + (views[i].virtual_address & 0x3FFFFFFF));
			if (!*views[i].out_ptr)
				goto bail;
		}
#endif
		last_position = position;
		position += views[i].size;
	}

	return true;

bail:
	// Argh! ERROR! Free what we grabbed so far so we can try again.
	MemoryMap_Shutdown(views, i+1, flags, arena);
	return false;
}

u8 *MemoryMap_Setup(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	u32 total_mem = 0;
	int base_attempts = 0;

	for (int i = 0; i < num_views; i++)
	{
		SKIP(flags, views[i].flags);
		if ((views[i].flags & MV_MIRROR_PREVIOUS) == 0)
			total_mem += views[i].size;
	}
	// Grab some pagefile backed memory out of the void ...
	arena->GrabLowMemSpace(total_mem);

	// Now, create views in high memory where there's plenty of space.
#ifdef _M_X64
	u8 *base = MemArena::Find4GBBase();
	// This really shouldn't fail - in 64-bit, there will always be enough
	// address space.
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return 0;
	}
#else
#ifdef _WIN32
	// Try a whole range of possible bases. Return once we got a valid one.
	u32 max_base_addr = 0x7FFF0000 - 0x31000000;
	u8 *base = NULL;

	for (u32 base_addr = 0x40000; base_addr < max_base_addr; base_addr += 0x40000)
	{
		base_attempts++;
		base = (u8 *)base_addr;
		if (Memory_TryBase(base, views, num_views, flags, arena)) 
		{
			INFO_LOG(MEMMAP, "Found valid memory base at %p after %i tries.", base, base_attempts);
			base_attempts = 0;
			break;
		}
		
	}
#else
	// Linux32 is fine with the x64 method, although limited to 32-bit with no automirrors.
	u8 *base = MemArena::Find4GBBase();
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return 0;
	}
#endif

#endif
	if (base_attempts)
		PanicAlert("No possible memory base pointer found!");
	return base;
}

void MemoryMap_Shutdown(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
	std::set<void*> freeset;
	for (int i = 0; i < num_views; i++)
	{
		const MemoryView* view = &views[i];
		u8** outptrs[2] = {view->out_ptr_low, view->out_ptr};
		for (int j = 0; j < 2; j++)
		{
			u8** outptr = outptrs[j];
			if (outptr && *outptr && !freeset.count(*outptr))
			{
				arena->ReleaseView(*outptr, view->size);
				freeset.insert(*outptr);
				*outptr = NULL;
			}
		}
	}
}
