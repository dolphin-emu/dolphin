// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace SamplerCommon
{

// Helper for checking if a BPMemory TexMode0 register is set to Point
// Filtering modes. This is used to decide whether Anisotropic enhancements
// are (mostly) safe in the VideoBackends.
// If both the minification and magnification filters are set to POINT modes
// then applying anisotropic filtering is equivalent to forced filtering. Point
// mode textures are usually some sort of 2D UI billboard which will end up
// misaligned from the correct pixels when filtered anisotropically.
template<class T>
constexpr bool IsBpTexMode0PointFiltering(const T& tm0)
{
	return tm0.min_filter < 4 && !tm0.mag_filter;
}

// Check if the minification filter has mipmap based filtering modes enabled.
template<class T>
constexpr bool AreBpTexMode0MipmapsEnabled(const T& tm0)
{
	return (tm0.min_filter & 3) != 0;
}

}
