// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Common/CommonTypes.h"

#undef OS  // CURL defines that, nobody uses it...

namespace DriverDetails
{
// API types supported by driver details
// This is separate to APIType in VideoConfig.h due to the fact that a bug
// can affect multiple APIs.
enum API
{
  API_OPENGL = (1 << 0),
  API_VULKAN = (1 << 1),
  API_METAL = (1 << 2),
};

// Enum of supported operating systems
enum OS
{
  OS_ALL = (1 << 0),
  OS_WINDOWS = (1 << 1),
  OS_LINUX = (1 << 2),
  OS_OSX = (1 << 3),
  OS_ANDROID = (1 << 4),
  OS_FREEBSD = (1 << 5),
  OS_OPENBSD = (1 << 6),
  OS_NETBSD = (1 << 7),
  OS_HAIKU = (1 << 8),
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
  VENDOR_APPLE,
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
  DRIVER_PORTABILITY,  // Vulkan via Metal on macOS
  DRIVER_APPLE,        // Metal on macOS
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
  BUG_BROKEN_UBO,
  // Bug: The pinned memory extension isn't working for index buffers
  // Affected devices: AMD as they are the only vendor providing this extension
  // Started Version: ?
  // Ended Version: 13.9 working for me (neobrain).
  // Affected OS: Linux
  // Pinned memory is disabled for index buffer as the AMD driver (the only one with pinned memory
  // support) seems
  // to be broken. We just get flickering/black rendering when using pinned memory here -- degasus -
  // 2013/08/20
  // This bug only happens when paired with base_vertex.
  // Please see issue #6105. Let's hope buffer storage solves this issue.
  // TODO: Detect broken drivers.
  BUG_BROKEN_PINNED_MEMORY,
  // Bug: glBufferSubData/glMapBufferRange stalls + OOM
  // Affected devices: Adreno a3xx/Mali-t6xx
  // Started Version: -1
  // Ended Version: -1
  // Both Adreno and Mali have issues when you call glBufferSubData or glMapBufferRange
  // The driver stalls in each instance no matter what you do
  // Apparently Mali and Adreno share code in this regard since they were written by the same
  // person.
  BUG_BROKEN_BUFFER_STREAM,
  // Bug: ARB_buffer_storage doesn't work with ARRAY_BUFFER type streams
  // Affected devices: GeForce 4xx+
  // Started Version: -1
  // Ended Version: 332.21
  // The buffer_storage streaming method is required for greater speed gains in our buffer streaming
  // It reduces what is needed for streaming to basically a memcpy call
  // It seems to work for all buffer types except GL_ARRAY_BUFFER
  BUG_BROKEN_BUFFER_STORAGE,
  // Bug: Intel HD 3000 on OS X has broken primitive restart
  // Affected devices: Intel HD 3000
  // Affected OS: OS X
  // Started Version: -1
  // Ended Version: -1
  // The drivers on OS X has broken primitive restart.
  // Intel HD 4000 series isn't affected by the bug
  BUG_PRIMITIVE_RESTART,
  // Bug: unsync mapping doesn't work fine
  // Affected devices: Nvidia driver, ARM Mali
  // Started Version: -1
  // Ended Version: -1
  // The Nvidia driver (both Windows + Linux) doesn't like unsync mapping performance wise.
  // Because of their threaded behavior, they seem not to handle unsync mapping complete unsync,
  // in fact, they serialize the driver which adds a much bigger overhead.
  // Workaround: Use BufferSubData
  // The Mali behavior is even worse: They just ignore the unsychronized flag and stall the GPU.
  // Workaround: As they were even too lazy to implement asynchronous buffer updates,
  //             BufferSubData stalls as well, so we have to use the slowest possible path:
  //             Alloc one buffer per draw call with BufferData.
  // TODO: some Windows AMD driver/GPU combination seems also affected
  //       but as they all support pinned memory, it doesn't matter
  BUG_BROKEN_UNSYNC_MAPPING,
  // Bug: Intel's Window driver broke buffer_storage with GL_ELEMENT_ARRAY_BUFFER
  // Affected devices: Intel (Windows)
  // Started Version: 15.36.3.64.3907 (10.18.10.3907)
  // Ended Version: 15.36.7.64.3960 (10.18.10.3960)
  // Intel implemented buffer_storage in their GL 4.3 driver.
  // It works for all the buffer types we use except GL_ELEMENT_ARRAY_BUFFER.
  // Causes complete blackscreen issues.
  BUG_INTEL_BROKEN_BUFFER_STORAGE,
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
  // The issue with this is that Intel's Windows driver is broken when checking if a boolean value
  // is equal to true or false, so one has to do a boolean negation of the source
  //
  // eg.
  // Broken on Qualcomm
  // Works on Windows Intel
  // if (!cond)
  //
  // Works on Qualcomm
  // Broken on Windows Intel
  // if (cond == false)
  BUG_BROKEN_NEGATED_BOOLEAN,

  // Bug: glCopyImageSubData doesn't work on i965
  // Started Version: -1
  // Ended Version: 10.6.4
  // Mesa meta misses to disable the scissor test.
  BUG_BROKEN_COPYIMAGE,

  // Bug: ARM Mali managed to break disabling vsync
  // Affected Devices: Mali
  // Started Version: r5p0-rev2
  // Ended Version: -1
  // If we disable vsync with eglSwapInterval(dpy, 0) then the screen will stop showing new updates
  // after a handful of swaps.
  // This was noticed on a Samsung Galaxy S6 with its Android 5.1.1 update.
  // The default Android 5.0 image didn't encounter this issue.
  // We can't actually detect what the driver version is on Android, so until the driver version
  // lands that displays the version in
  // the GL_VERSION string, we will have to force vsync to be enabled at all times.
  BUG_BROKEN_VSYNC,

  // Bug: Broken lines in geometry shaders
  // Affected Devices: Mesa r600/radeonsi, Mesa Sandy Bridge
  // Started Version: -1
  // Ended Version: 11.1.2 for radeon, -1 for Sandy
  // Mesa introduced geometry shader support for radeon and sandy bridge devices and failed to test
  // it with us.
  // Causes misrenderings on a large amount of things that draw lines.
  BUG_BROKEN_GEOMETRY_SHADERS,

  // Bug: glGetBufferSubData for bounding box reads is slow on AMD drivers
  // Started Version: -1
  // Ended Version: -1
  // Bounding box reads use glGetBufferSubData to read back the contents of the SSBO, but this is
  // slow on AMD drivers, compared to
  // using glMapBufferRange. glMapBufferRange is slower on Nvidia drivers, we suspect due to the
  // first call moving the buffer from
  // GPU memory to system memory. Use glMapBufferRange for BBox reads on AMD, and glGetBufferSubData
  // everywhere else.
  BUG_SLOW_GETBUFFERSUBDATA,

  // Bug: Broken lines in geometry shaders when writing to gl_ClipDistance in the vertex shader
  // Affected Devices: Mesa i965
  // Started Version: -1
  // Ended Version: -1
  // Writing to gl_ClipDistance in both the vertex shader and the geometry shader will break
  // the geometry shader. Current workaround is to make sure the geometry shader always consumes
  // the gl_ClipDistance inputs from the vertex shader.
  BUG_BROKEN_CLIP_DISTANCE,

  // Bug: Dual-source outputs from fragment shaders are broken on some drivers.
  // Started Version: -1
  // Ended Version: -1
  // On some AMD drivers, fragment shaders that specify dual-source outputs can cause the driver to
  // crash. Sometimes this happens in the kernel mode part of the driver, resulting in a BSOD.
  // These shaders are also particularly problematic on macOS's Intel drivers. On OpenGL, they can
  // cause depth issues. On Metal, they can cause the driver to not write a primitive to the depth
  // buffer if dual source blending is output in the shader but not subsequently used in blending.
  // Compile separate shaders for DSB on vs off for these drivers.
  BUG_BROKEN_DUAL_SOURCE_BLENDING,

  // BUG: ImgTec GLSL shader compiler fails when negating the input to a bitwise operation
  // Started version: 1.5
  // Ended version: 1.8@4693462
  // Shaders that do something like "variable <<= (-othervariable);" cause the shader to
  // fail compilation with no useful diagnostic log. This can be worked around by storing
  // the negated value to a temporary variable then using that in the bitwise op.
  BUG_BROKEN_BITWISE_OP_NEGATION,

  // BUG: The GPU shader code appears to be context-specific on Mesa/i965.
  // This means that if we compiled the ubershaders asynchronously, they will be recompiled
  // on the main thread the first time they are used, causing stutter. For now, disable
  // asynchronous compilation on Mesa i965. On nouveau, our use of glFinish() can cause
  // crashes and/or lockups.
  // Started version: -1
  // Ended Version: -1
  BUG_SHARED_CONTEXT_SHADER_COMPILATION,

  // Bug: Fast clears on a MSAA framebuffer can cause NVIDIA GPU resets/lockups.
  // Started version: -1
  // Ended version: -1
  // Calling vkCmdClearAttachments with a partial rect, or specifying a render area in a
  // render pass with the load op set to clear can cause the GPU to lock up, or raise a
  // bounds violation. This only occurs on MSAA framebuffers, and it seems when there are
  // multiple clears in a single command buffer. Worked around by back to the slow path
  // (drawing quads) when MSAA is enabled.
  BUG_BROKEN_MSAA_CLEAR,

  // BUG: Some vulkan implementations don't like the 'clear' loadop renderpass.
  // For example, the ImgTec VK driver fails if you try to use a framebuffer with a different
  // load/store op than that which it was created with, despite the spec saying they should be
  // compatible.
  // Started Version: 1.7
  // Ended Version: 1.10
  BUG_BROKEN_CLEAR_LOADOP_RENDERPASS,

  // BUG: 32-bit depth clears are broken in the Adreno Vulkan driver, and have no effect.
  // To work around this, we use a D24_S8 buffer instead, which results in a loss of accuracy.
  // We still resolve this to a R32F texture, as there is no 24-bit format.
  // Started version: -1
  // Ended version: -1
  BUG_BROKEN_D32F_CLEAR,

  // BUG: Reversed viewport depth range does not work as intended on some Vulkan drivers.
  // The Vulkan spec allows the minDepth/maxDepth fields in the viewport to be reversed,
  // however the implementation is broken on some drivers.
  BUG_BROKEN_REVERSED_DEPTH_RANGE,

  // BUG: Cached memory is significantly slower for readbacks than coherent memory in the
  // Mali Vulkan driver, causing high CPU usage in the __pi___inval_cache_range kernel
  // function. This flag causes readback buffers to select the coherent type.
  BUG_SLOW_CACHED_READBACK_MEMORY,

  // BUG: Apparently ARM Mali GLSL compiler managed to break bitwise AND operations between
  // two integers vectors, when one of them is non-constant (though the exact cases of when
  // this occurs are still unclear). The resulting vector from the operation will be the
  // constant vector.
  // Easy enough to fix, just do the bitwise AND operation on the vector components first and
  // then construct the final vector.
  // Started version: -1
  // Ended version: -1
  BUG_BROKEN_VECTOR_BITWISE_AND,

  // BUG: Accessing gl_SubgroupInvocationID causes the Metal shader compiler to crash.
  //      Affected devices: AMD (older macOS)
  // BUG: gl_HelperInvocation always returns true, even for non-helper invocations
  //      Affected devices: AMD (newer macOS)
  // BUG: Using subgroupMax in a shader that can discard results in garbage data
  //      (For some reason, this only happens at 4x+ IR on Metal, but 2x+ IR on MoltenVK)
  //      Affected devices: Intel (macOS)
  // Started version: -1
  // Ended version: -1
  BUG_BROKEN_SUBGROUP_OPS,

  // BUG: Multi-threaded shader pre-compilation sometimes crashes
  // Used primarily in Videoconfig.cpp's GetNumAutoShaderPreCompilerThreads()
  // refer to https://github.com/dolphin-emu/dolphin/pull/9414 for initial validation coverage
  BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION,

  // BUG: Some driver and Apple Silicon GPU combinations have problems with fragment discard when
  // early depth test is enabled. Discarded fragments may appear corrupted (Super Mario Sunshine,
  // Sonic Adventure 2: Battle, Phantasy Star Online Epsiodes 1 & 2, etc).
  // Affected devices: Apple Silicon GPUs of Apple family 4 and newer.
  // Started version: -1
  // Ended version: -1
  BUG_BROKEN_DISCARD_WITH_EARLY_Z,

  // BUG: Using dynamic sampler indexing locks up the GPU
  // Affected devices: Intel (macOS Metal)
  // Started version: -1
  // Ended version: -1
  BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING,

  // BUG: vkCmdCopyImageToBuffer allocates a staging image when used to copy from
  // an image with optimal tiling.
  // Affected devices: Adreno
  // Started Version: -1
  // Ended Version: -1
  BUG_SLOW_OPTIMAL_IMAGE_TO_BUFFER_COPY
};

// Initializes our internal vendor, device family, and driver version
void Init(API api, Vendor vendor, Driver driver, const double version, const Family family);

// Once Vendor and driver version is set, this will return if it has the applicable bug passed to
// it.
bool HasBug(Bug bug);
}  // namespace DriverDetails
