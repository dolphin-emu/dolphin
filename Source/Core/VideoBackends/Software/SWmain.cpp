// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/VideoInterface.h"

#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

#define VSYNC_ENABLED 0

namespace SW
{

class PerfQuery : public PerfQueryBase
{
public:
	PerfQuery() {}
	~PerfQuery() {}

	void EnableQuery(PerfQueryGroup type) override {}
	void DisableQuery(PerfQueryGroup type) override {}
	void ResetQuery() override
	{
		memset(EfbInterface::perf_values, 0, sizeof(EfbInterface::perf_values));
	}
	u32 GetQueryResult(PerfQueryType type) override
	{
		return EfbInterface::perf_values[type];
	};
	void FlushResults() override {}
	bool IsFlushed() const override { return true; };
};

class TextureCache : public TextureCacheBase
{
public:
	void CompileShaders() override {};
	void DeleteShaders() override {};
	void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format) override {};
	void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) override
	{
		EfbCopy::CopyEfb();
	}

private:
	struct TCacheEntry : TCacheEntryBase
	{
		TCacheEntry(const TCacheEntryConfig& _config) : TCacheEntryBase(_config) {}
		~TCacheEntry() {}

		void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level) override {}

		void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) override
		{
			EfbCopy::CopyEfb();
		}

		void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int>& srcrect,
			const MathUtil::Rectangle<int>& dstrect) override {}

		void Bind(unsigned int stage, unsigned int last_texture) override {}

		bool Save(const std::string& filename, unsigned int level) override { return false; }
	};

	TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override
	{
		return new TCacheEntry(config);
	}
};

class XFBSource : public XFBSourceBase
{
	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override {}
	void CopyEFB(float Gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override { return std::make_unique<XFBSource>(); }
	void GetTargetSize(unsigned int* width, unsigned int* height) override {};
	void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma = 1.0f) override
	{
		EfbCopy::CopyEfb();
	}
};

std::string VideoSoftware::GetName() const
{
	return "Software Renderer";
}

std::string VideoSoftware::GetDisplayName() const
{
	return "Software Renderer";
}

static void InitBackendInfo()
{
	g_Config.backend_info.APIType = API_NONE;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsEarlyZ = true;
	g_Config.backend_info.bSupportsOversizedViewports = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = false;

	// aamodes
	g_Config.backend_info.AAModes = {1};
}

void VideoSoftware::ShowConfig(void *hParent)
{
	if (!m_initialized)
		InitBackendInfo();
	Host_ShowVideoConfig(hParent, GetDisplayName(), "gfx_software");
}

bool VideoSoftware::Initialize(void *window_handle)
{
	InitializeShared();
	InitBackendInfo();

	g_Config.Load((File::GetUserPath(D_CONFIG_IDX) + "gfx_software.ini").c_str());
	g_Config.GameIniLoad();
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	SWOGLWindow::Init(window_handle);

	PixelEngine::Init();
	Clipper::Init();
	Rasterizer::Init();
	SWRenderer::Init();
	DebugUtil::Init();

	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::CallbackType::Initialization);

	m_initialized = true;

	return true;
}

void VideoSoftware::Shutdown()
{
	m_initialized = false;

	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::CallbackType::Shutdown);

	SWOGLWindow::Shutdown();
}

void VideoSoftware::Video_Cleanup()
{
	if (g_renderer)
	{
		Fifo::Shutdown();
		SWRenderer::Shutdown();
		DebugUtil::Shutdown();
		// The following calls are NOT Thread Safe
		// And need to be called from the video thread
		SWRenderer::Shutdown();
		VertexLoaderManager::Shutdown();
		g_framebuffer_manager.reset();
		g_texture_cache.reset();
		VertexShaderManager::Shutdown();
		PixelShaderManager::Shutdown();
		g_perf_query.reset();
		g_vertex_manager.reset();
		OpcodeDecoder_Shutdown();
		g_renderer.reset();
	}
}

// This is called after Video_Initialize() from the Core
void VideoSoftware::Video_Prepare()
{
	g_renderer = std::make_unique<SWRenderer>();

	CommandProcessor::Init();
	PixelEngine::Init();

	BPInit();
	g_vertex_manager = std::make_unique<SWVertexLoader>();
	g_perf_query = std::make_unique<PerfQuery>();
	Fifo::Init(); // must be done before OpcodeDecoder_Init()
	OpcodeDecoder_Init();
	IndexGenerator::Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	g_texture_cache = std::make_unique<TextureCache>();
	SWRenderer::Init();
	VertexLoaderManager::Init();
	g_framebuffer_manager = std::make_unique<FramebufferManager>();

	// Notify the core that the video backend is ready
	Host_Message(WM_USER_CREATE);

	INFO_LOG(VIDEO, "Video backend initialized.");
}

unsigned int VideoSoftware::PeekMessages()
{
	return SWOGLWindow::s_instance->PeekMessages();
}

}
