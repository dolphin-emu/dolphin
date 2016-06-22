// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/VulkanImports.h"
#include "VideoBackends/Vulkan/Texture2D.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;

class XFBSource : public XFBSourceBase
{
	void DecodeToTexture(u32 xfb_addr, u32 fb_width, u32 fb_height) override {}
	void CopyEFB(float gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
public:
	FramebufferManager(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr);
	~FramebufferManager();

	VkRenderPass GetEFBRenderPass() const { return m_efb_render_pass; }

	u32 GetEFBWidth() const { return m_efb_width; }
	u32 GetEFBHeight() const { return m_efb_height; }
	u32 GetEFBLayers() const { return m_efb_layers; }

	Texture2D* GetEFBColorTexture() const { return m_efb_color_texture.get(); }
	Texture2D* GetEFBDepthTexture() const { return m_efb_depth_texture.get(); }
	VkFramebuffer GetEFBFramebuffer() const { return m_efb_framebuffer; }

	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override { return std::make_unique<XFBSource>(); }
	void GetTargetSize(unsigned int* width, unsigned int* height) override {};
	void CopyToRealXFB(u32 xfb_addr, u32 fb_stride, u32 fb_height, const EFBRectangle& source_rc, float gamma = 1.0f) override {}

	void ResizeEFBTextures();

private:
	bool CreateEFBRenderPass();
	void DestroyEFBRenderPass();
	bool CreateEFBFramebuffer();
	void DestroyEFBFramebuffer();

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;

	VkRenderPass m_efb_render_pass = VK_NULL_HANDLE;

	u32 m_efb_width = 0;
	u32 m_efb_height = 0;
	u32 m_efb_layers = 1;

	std::unique_ptr<Texture2D> m_efb_color_texture;
	std::unique_ptr<Texture2D> m_efb_depth_texture;
	VkFramebuffer m_efb_framebuffer = VK_NULL_HANDLE;
};

}		// namespace Vulkan
