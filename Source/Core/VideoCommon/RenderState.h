// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BPStructs.h"

enum class PrimitiveType : u32
{
  Points,
  Lines,
  Triangles,
};

union RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  RasterizationState& operator=(const RasterizationState& rhs);

  bool operator==(const RasterizationState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const RasterizationState& rhs) const { return hex != rhs.hex; }
  bool operator<(const RasterizationState& rhs) const { return hex < rhs.hex; }


	struct {
		GenMode::CullMode cullmode:2;
		PrimitiveType primitive:2;
	};
	u32 hex;
};

union DepthState
{
  void Generate(const BPMemory& bp);

  DepthState& operator=(const DepthState& rhs);

  bool operator==(const DepthState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const DepthState& rhs) const { return hex != rhs.hex; }
  bool operator<(const DepthState& rhs) const { return hex < rhs.hex; }

	struct {
		unsigned int testenable:1;
		unsigned int updateenable:1;
		ZMode::CompareMode func:3;
	};
	u32 hex;
};

union BlendingState
{
  void Generate(const BPMemory& bp);

  bool IsDualSourceBlend() const {
    return dstalpha && (srcfactor == BlendMode::SRCALPHA || srcfactor == BlendMode::INVSRCALPHA);
  }

  bool IsPremultipliedAlpha() const {
    return false && !dstalpha && (srcfactor == BlendMode::SRCALPHA || srcfactor == BlendMode::INVSRCALPHA);
  }

  BlendingState& operator=(const BlendingState& rhs);

  bool operator==(const BlendingState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const BlendingState& rhs) const { return hex != rhs.hex; }
  bool operator<(const BlendingState& rhs) const { return hex < rhs.hex; }

	struct {
		unsigned int blendenable:1;
		unsigned int logicopenable:1;
		unsigned int dstalpha:1;
		unsigned int colorupdate:1;
		unsigned int alphaupdate:1;
		unsigned int subtract:1;
		unsigned int subtractAlpha:1;
		BlendMode::BlendFactor dstfactor:3;
		BlendMode::BlendFactor srcfactor:3;
		BlendMode::BlendFactor dstfactoralpha:3;
		BlendMode::BlendFactor srcfactoralpha:3;
		BlendMode::LogicOp logicmode:4;
	};
	u32 hex;
};

union SamplerState
{
  using StorageType = u64;

  enum class Filter : u32
  {
    Point,
    Linear
  };

  enum class AddressMode : u32
  {
    Clamp,
    Repeat,
    MirroredRepeat
  };

  void Generate(const BPMemory& bp, u32 index);

  SamplerState& operator=(const SamplerState& rhs);

  bool operator==(const SamplerState& rhs) const { return hex == rhs.hex; }
  bool operator!=(const SamplerState& rhs) const { return hex != rhs.hex; }
  bool operator<(const SamplerState& rhs) const { return hex < rhs.hex; }


	struct {
		unsigned int anisotropic_filtering:1;
		Filter min_filter:1;
		Filter mag_filter:1;
		Filter mipmap_filter:1;
		AddressMode wrap_u:2;
		AddressMode wrap_v:2;
		unsigned int min_lod:8;   // multiplied by 16
		unsigned int max_lod:8;   // multiplied by 16
		int lod_bias:16;  // multiplied by 256
	};
  StorageType hex;
};

namespace RenderState
{
RasterizationState GetNoCullRasterizationState();
DepthState GetNoDepthTestingDepthStencilState();
BlendingState GetNoBlendingBlendState();
SamplerState GetPointSamplerState();
SamplerState GetLinearSamplerState();
}  // namespace RenderState
