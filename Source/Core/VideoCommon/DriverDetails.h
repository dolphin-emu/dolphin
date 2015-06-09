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

	// Enum of known bugs
	// These can be vendor specific, but we put them all in here
	// For putting a new bug in here, make sure to put a detailed comment above the enum
	// This'll ensure we know exactly what the issue is.
	enum Bug
	{
		// Bug: No Dynamic UBO array object access
		// Affected Devices: Qualcomm/Adreno
		// Started Version: 14
		// Ended Version: 95
		// Accessing UBO array members dynamically causes the Adreno shader compiler to crash
		// Errors out with "Internal Error"
		// With v53 video drivers, dynamic member access "works." It works to the extent that it doesn't crash.
		// With v95 drivers everything works as it should.
		BUG_NODYNUBOACCESS = 0,
		// Bug: Centroid is broken in shaders
		// Affected devices: Qualcomm/Adreno
		// Started Version: 14
		// Ended Version: 53
		// Centroid in/out, used in the shaders, is used for multisample buffers to get the texel correctly
		// When MSAA is disabled, it acts like a regular in/out
		// Tends to cause the driver to render full white or black
		BUG_BROKENCENTROID,
		// Bug: INFO_LOG_LENGTH broken
		// Affected devices: Qualcomm/Adreno
		// Started Version: ? (Noticed on v14)
		// Ended Version: 53
		// When compiling a shader, it is important that when it fails,
		// you first get the length of the information log prior to grabbing it.
		// This allows you to allocate an array to store all of the log
		// Adreno devices /always/ return 0 when querying GL_INFO_LOG_LENGTH
		// They also max out at 1024 bytes(1023 characters + null terminator) for the log
		BUG_BROKENINFOLOG,
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
		// Please see issue #6105 on Google Code. Let's hope buffer storage solves this issues.
		// TODO: Detect broken drivers.
		BUG_BROKENPINNEDMEMORY,
		// Bug: Entirely broken UBOs
		// Affected devices: Qualcomm/Adreno
		// Started Version: ? (Noticed on v45)
		// Ended Version: 53
		// Uniform buffers are entirely broken on Qualcomm drivers with v45
		// Trying to use the uniform buffers causes a malloc to fail inside the driver
		// To be safe, blanket drivers from v41 - v45
		BUG_ANNIHILATEDUBOS,
		// Bug : Can't draw on screen text and clear correctly.
		// Affected devices: Qualcomm/Adreno
		// Started Version: ?
		// Ended Version: 53
		// Current code for drawing on screen text and clearing the framebuffer doesn't work on Adreno
		// Drawing on screen text causes the whole screen to swizzle in a terrible fashion
		// Clearing the framebuffer causes one to never see a frame.
		BUG_BROKENSWAP,
		// Bug: glBufferSubData/glMapBufferRange stalls + OOM
		// Affected devices: Adreno a3xx/Mali-t6xx
		// Started Version: -1
		// Ended Version: -1
		// Both Adreno and Mali have issues when you call glBufferSubData or glMapBufferRange
		// The driver stalls in each instance no matter what you do
		// Apparently Mali and Adreno share code in this regard since it was wrote by the same person.
		BUG_BROKENBUFFERSTREAM,
		// Bug: GLSL ES 3.0 textureSize causes abort
		// Affected devices: Adreno a3xx
		// Started Version: -1 (Noticed in v53)
		// Ended Version: 66
		// If a shader includes a textureSize function call then the shader compiler will call abort()
		BUG_BROKENTEXTURESIZE,
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
		// Bug: Qualcomm has broken attributeless rendering
		// Affected devices: Adreno
		// Started Version: -1
		// Ended Version: v66 (07-09-2014 dev version), v95 shipping
		// Qualcomm has had attributeless rendering broken forever
		// This was fixed in a v66 development version, the first shipping driver version with the release was v95.
		// To be safe, make v95 the minimum version to work around this issue
		BUG_BROKENATTRIBUTELESS,
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

		// Bug: Qualcomm has broken ivec to scalar and ivec to ivec bitshifts
		// Affected devices: Adreno
		// Started Version: -1
		// Ended Version: 46 (TODO: Test more devices, the real end is currently unknown)
		// Qualcomm has broken integer vector to integer bitshifts, and integer vector to integer vector bitshifts
		// A compilation error is generated when trying to compile the shaders.
		//
		// For example:
		//	Broken on Qualcomm:
		//		ivec4 ab = ivec4(1,1,1,1);
		//		ab <<= 2;
		//
		//	Working on Qualcomm:
		//		ivec4 ab = ivec4(1,1,1,1);
		//		ab.x <<= 2;
		//		ab.y <<= 2;
		//		ab.z <<= 2;
		//		ab.w <<= 2;
		//
		//	Broken on Qualcomm:
		//		ivec4 ab = ivec4(1,1,1,1);
		//		ivec4 cd = ivec4(1,2,3,4);
		//		ab <<= cd;
		//
		//	Working on Qualcomm:
		//		ivec4 ab = ivec4(1,1,1,1);
		//		ivec4 cd = ivec4(1,2,3,4);
		//		ab.x <<= cd.x;
		//		ab.y <<= cd.y;
		//		ab.z <<= cd.z;
		//		ab.w <<= cd.w;
		BUG_BROKENIVECSHIFTS,

		// Bug: Intel crashes on Windows on loading some shaders.
		// Affected devices: all Intel GPUs on Windows
		// Started Version: -1
		// Ended Version: -1
		BUG_BROKENSHADERCACHE,
	};

	// Initializes our internal vendor, device family, and driver version
	void Init(Vendor vendor, Driver driver, const double version, const s32 family);

	// Once Vendor and driver version is set, this will return if it has the applicable bug passed to it.
	bool HasBug(Bug bug);
}
