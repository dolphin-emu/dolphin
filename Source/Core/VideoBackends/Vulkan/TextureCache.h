// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/StreamBuffer.h"

#include "VideoCommon/TextureCacheBase.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;
class StateTracker;
class Texture2D;

class TextureCache : public TextureCacheBase
{
public:
	TextureCache(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker);
	~TextureCache();

	void CompileShaders() override;
	void DeleteShaders() override;
	void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format) override;

	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
		bool is_intensity, bool scale_by_half) override;

private:
	struct TCacheEntry : TCacheEntryBase
	{
		TCacheEntry(const TCacheEntryConfig& _config, TextureCache* parent, std::unique_ptr<Texture2D> texture);
		~TCacheEntry();

		void Load(unsigned int width, unsigned int height, unsigned int expanded_width, unsigned int level) override;
		void FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect, bool scale_by_half, unsigned int cbufid, const float* colmat) override;
		void CopyRectangleFromTexture(const TCacheEntryBase* source, const MathUtil::Rectangle<int>& srcrect, const MathUtil::Rectangle<int>& dstrect) override;

		void Bind(unsigned int stage) override;
		bool Save(const std::string& filename, unsigned int level) override;

	private:
		TextureCache* m_parent;
		std::unique_ptr<Texture2D> m_texture;
	};

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;
	StateTracker* m_state_tracker = nullptr;

	std::unique_ptr<StreamBuffer> m_texture_upload_buffer = nullptr;
};

} // namespace Vulkan
