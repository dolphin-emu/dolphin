// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once
#include "Common/CommonTypes.h"

namespace DriverDetails
{
	// Enum of supported operating systems
	enum OS
	{
		OS_ALL     = (1 << 0),
		OS_WINDOWS = (1 << 1),
		OS_LINUX   = (1 << 2),
		OS_OSX     = (1 << 3),
		OS_ANDROID = (1 << 4),
		OS_FREEBSD = (1 << 5),
	};
	// Enum of known vendors
	// Tegra and Nvidia are separated out due to such substantial differences
	enum Vendor
	{
		VENDOR_ALL = 0,
		VENDOR_NVIDIA,
		VENDOR_ATI,
		VENDOR_INTEL,
		VENDOR_ARM,
		VENDOR_QUALCOMM,
		VENDOR_IMGTEC,
		VENDOR_TEGRA,
		VENDOR_VIVANTE,
		VENDOR_MESA,
		VENDOR_UNKNOWN
	};

	// Enum of known drivers
	enum Driver
	{
		DRIVER_ALL = 0,
		DRIVER_NVIDIA,       // Official Nvidia, including mobile GPU
		DRIVER_NOUVEAU,      // OSS nouveau
		DRIVER_ATI,          // Official ATI
		DRIVER_R600,         // OSS Radeon
		DRIVER_INTEL,        // Official Intel
		DRIVER_I965,         // OSS Intel
		DRIVER_ARM,          // Official Mali driver
		DRIVER_LIMA,         // OSS Mali driver
		DRIVER_QUALCOMM,     // Official Adreno driver
		DRIVER_FREEDRENO,    // OSS Adreno driver
		DRIVER_IMGTEC,       // Official PowerVR driver
		DRIVER_VIVANTE,      // Official Vivante driver
		DRIVER_UNKNOWN       // Unknown driver, default to official hardware driver
	};

	enum class Family
	{
		UNKNOWN,
		INTEL_SANDY,
		INTEL_IVY,
	};

	// Enum of known bugs
	// These can be vendor specific, but we put them all in here
	// For putting a new bug in here, make sure to put a detailed comment above the enum
	// This'll ensure we know exactly what the issue is.
	enum Bug
	{
		// Bug: UBO buffer offset broken
		// Affected devices: all mesa drivers
		// Started Version: 9.0 (mesa doesn't support ubo before)
		// Ended Version: up to 9.2
		// The offset of glBindBufferRange was ignored on all Mesa Gallium3D drivers until 9.1.3
		// Nouveau stored the offset as u16 which isn't enough for all cases with range until 9.1.6
		// I965 has broken data fetches from uniform buffers which results in a dithering until 9.2.0
		BUG_BROKENUBO,
		// Bug: The pinned memory extension isn't working for index buffers
		// Affected devices: AMD as they are the only vendor providing this extension
		// Started Version: ?
		// Ended Version: 13.9 working for me (neobrain).
		// Affected OS: Linux
		// Pinned memory is disabled for index buffer as the AMD driver (the only one with pinned memory support) seems
		// to be broken. We just get flickering/black rendering when using pinned memory here -- degasus - 2013/08/20
		// This bug only happens when paired with base_vertex.
		// Please see issue #6105. Let's hope buffer storage solves this issue.
		// TODO: Detect broken drivers.
		BUG_BROKENPINNEDMEMORY,
		// Bug: glBufferSubData/glMapBufferRange stalls + OOM
		// Affected devices: Adreno a3xx/Mali-t6xx
		// Started Version: -1
		// Ended Version: -1
		// Both Adreno and Mali have issues when you call glBufferSubData or glMapBufferRange
		// The driver stalls in each instance no matter what you do
		// Apparently Mali and Adreno share code in this regard since it was wrote by the same person.
		BUG_BROKENBUFFERSTREAM,
		// Bug: ARB_buffer_storage doesn't work with ARRAY_BUFFER type streams
		// Affected devices: GeForce 4xx+
		// Started Version: -1
		// Ended Version: 332.21
		// The buffer_storage streaming method is required for greater speed gains in our buffer streaming
		// It reduces what is needed for streaming to basically a memcpy call
		// It seems to work for all buffer types except GL_ARRAY_BUFFER
		BUG_BROKENBUFFERSTORAGE,
		// Bug: Intel HD 3000 on OS X has broken primitive restart
		// Affected devices: Intel HD 3000
		// Affected OS: OS X
		// Started Version: -1
		// Ended Version: -1
		// The drivers on OS X has broken primitive restart.
		// Intel HD 4000 series isn't affected by the bug
		BUG_PRIMITIVERESTART,
		// Bug: unsync mapping doesn't work fine
		// Affected devices: Nvidia driver
		// Started Version: -1
		// Ended Version: -1
		// The Nvidia driver (both Windows + Linux) doesn't like unsync mapping performance wise.
		// Because of their threaded behavior, they seem not to handle unsync mapping complete unsync,
		// in fact, they serialize the driver which adds a much bigger overhead.
		// Workaround: Use BufferSubData
		// TODO: some Windows AMD driver/GPU combination seems also affected
		//       but as they all support pinned memory, it doesn't matter
		BUG_BROKENUNSYNCMAPPING,
		// Bug: Intel's Window driver broke buffer_storage with GL_ELEMENT_ARRAY_BUFFER
		// Affected devices: Intel (Windows)
		// Started Version: 15.36.3.64.3907 (10.18.10.3907)
		// Ended Version: 15.36.7.64.3960 (10.18.10.3960)
		// Intel implemented buffer_storage in their GL 4.3 driver.
		// It works for all the buffer types we use except GL_ELEMENT_ARRAY_BUFFER.
		// Causes complete blackscreen issues.
		BUG_INTELBROKENBUFFERSTORAGE,
		// Bug: Qualcomm has broken boolean negation
		// Affected devices: Adreno
		// Started Version: -1
		// Ended Version: -1
		// Qualcomm has the boolean negation broken in their shader compiler
		// Instead of inverting the boolean value it does a binary negation on the full 32bit register
		// This causes a compare against zero to fail in their shader since it is no longer a 0 or 1 value
		// but 0xFFFFFFFF or 0xFFFFFFFE depending on what the boolean value was before the negation.
		//
		// This bug has a secondary issue tied to it unlike other bugs.
		// The correction of this bug is to check the boolean value against false which results in us
		// not doing a negation of the source but instead checking against the boolean value we want.
		// The issue with this is that Intel's Window driver is broken when checking if a boolean value is
		// equal to true or false, so one has to do a boolean negation of the source
		//
		// eg.
		// Broken on Qualcomm
		// Works on Windows Intel
		// if (!cond)
		//
		// Works on Qualcomm
		// Broken on Windows Intel
		// if (cond == false)
		BUG_BROKENNEGATEDBOOLEAN,

		// Bug: glCopyImageSubData doesn't work on i965
		// Started Version: -1
		// Ended Version: 10.6.4
		// Mesa meta misses to disable the scissor test.
		BUG_BROKENCOPYIMAGE,

		// Bug: ARM Mali managed to break disabling vsync
		// Affected Devices: Mali
		// Started Version: r5p0-rev2
		// Ended Version: -1
		// If we disable vsync with eglSwapInterval(dpy, 0) then the screen will stop showing new updates after a handful of swaps.
		// This was noticed on a Samsung Galaxy S6 with its Android 5.1.1 update.
		// The default Android 5.0 image didn't encounter this issue.
		// We can't actually detect what the driver version is on Android, so until the driver version lands that displays the version in
		// the GL_VERSION string, we will have to force vsync to be enabled at all times.
		BUG_BROKENVSYNC,

		// Bug: Broken lines in geometry shaders
		// Affected Devices: Mesa r600/radeonsi, Mesa Sandy Bridge
		// Started Version: -1
		// Ended Version: 11.1.2 for radeon, -1 for Sandy
		// Mesa introduced geometry shader support for radeon and sandy bridge devices and failed to test it with us.
		// Causes misrenderings on a large amount of things that draw lines.
		BUG_BROKENGEOMETRYSHADERS,

		// Bug: Explicit flush is very slow on Qualcomm
		// Started Version: -1
		// Ended Version: -1
		// Our ARB_buffer_storage code uses explicit flush to avoid coherent mapping.
		// Qualcomm seems to have lots of overhead on exlicit flushing, but the coherent mapping path is fine.
		// So let's use coherent mapping there.
		BUG_BROKENEXPLICITFLUSH,

		// Bug: glGetBufferSubData for bounding box reads is slow on AMD drivers
		// Started Version: -1
		// Ended Version: -1
		// Bounding box reads use glGetBufferSubData to read back the contents of the SSBO, but this is slow on AMD drivers, compared to
		// using glMapBufferRange. glMapBufferRange is slower on Nvidia drivers, we suspect due to the first call moving the buffer from
		// GPU memory to system memory. Use glMapBufferRange for BBox reads on AMD, and glGetBufferSubData everywhere else.
		BUG_SLOWGETBUFFERSUBDATA,
	};

	// Initializes our internal vendor, device family, and driver version
	void Init(Vendor vendor, Driver driver, const double version, const Family family);

	// Once Vendor and driver version is set, this will return if it has the applicable bug passed to it.
	bool HasBug(Bug bug);
}
