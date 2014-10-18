// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
		DRIVER_ARM_MIDGARD,  // Official Mali driver
		DRIVER_ARM_UTGARD,   // Official Mali driver
		DRIVER_LIMA,         // OSS Mali driver
		DRIVER_QUALCOMM_3XX, // Official Adreno driver 3xx
		DRIVER_QUALCOMM_2XX, // Official Adreno driver 2xx
		DRIVER_FREEDRENO,    // OSS Adreno driver
		DRIVER_IMGTEC,       // OSS PowerVR driver
		DRIVER_VIVANTE,      // Official vivante driver
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
		// Pinned memory is disabled for index buffer as the amd driver (the only one with pinned memory support) seems
		// to be broken. We just get flickering/black rendering when using pinned memory here -- degasus - 2013/08/20
		// This bug only happens when paired with base_vertex.
		// Please see issue #6105 on google code. Let's hope buffer storage solves this issues.
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
		// Affected devices: Geforce 4xx+
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
		// Affected devices: nvidia driver
		// Started Version: -1
		// Ended Version: -1
		// The nvidia driver (both windows + linux) doesn't like unsync mapping performance wise.
		// Because of their threaded behavoir, they seem not to handle unsync mapping complete unsync,
		// in fact, they serialize the driver which adds a much bigger overhead.
		// Workaround: Use BufferSubData
		// TODO: some windows AMD driver/gpu combination seems also affected
		//       but as they all support pinned memory, it doesn't matter
		BUG_BROKENUNSYNCMAPPING,
		// Bug: Adreno now rotates the framebuffer on blit a full 180 degrees
		// Affected devices: Adreno
		// Started Version: v53 (dev drivers)
		// Ended Version: v66 (07-14-2014 dev drivers)
		// Qualcomm is a super pro company that has recently updated their development drivers
		// These drivers are available to the Nexus 5 and report as v53
		// Qualcomm in their infinite wisdom thought it was a good idea to rotate the framebuffer 180 degrees on glBlit
		// This bug allows us to work around that rotation by rotating it the right way around again.
		BUG_ROTATEDFRAMEBUFFER,
		// Bug: Intel's Window driver broke buffer_storage with GL_ELEMENT_ARRAY_BUFFER
		// Affected devices: Intel (Windows)
		// Started Version: 15.36.3.64.3907 (10.18.10.3907)
		// Ended Version: 15.36.7.64.3960 (10.18.10.3960)
		// Intel implemented buffer_storage in their GL 4.3 driver.
		// It works for all the buffer types we use except GL_ELEMENT_ARRAY_BUFFER.
		// Causes complete blackscreen issues.
		BUG_INTELBROKENBUFFERSTORAGE,
	};

	// Initializes our internal vendor, device family, and driver version
	void Init(Vendor vendor, Driver driver, const double version, const s32 family);

	// Once Vendor and driver version is set, this will return if it has the applicable bug passed to it.
	bool HasBug(Bug bug);
}
