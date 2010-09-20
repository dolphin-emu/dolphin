
#include "pluginspecs_video.h"

// TODO: temporary, just used in Video_Prepare
// comment out to use (try to use :p) OpenGL
#define USE_DX11

#include <wx/wx.h>
#include <wx/notebook.h>

// Common
#include "Common.h"
#include "Atomic.h"
#include "Thread.h"
#include "LogManager.h"

// VideoCommon
#include "VideoConfig.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "VertexLoaderManager.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "OnScreenDisplay.h"
#include "VideoState.h"
#include "XFBConvert.h"
#include "DLCache.h"

// internal crap
#include "Renderer.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "FramebufferManager.h"

#ifdef _WIN32

#include "DX11/DX11_TextureCache.h"
#include "DX11/DX11_VertexShaderCache.h"
#include "DX11/DX11_PixelShaderCache.h"
#include "DX11/DX11_Render.h"
#include "DX11/DX11_VertexManager.h"

#include "DX9/DX9_TextureCache.h"
#include "DX9/DX9_VertexShaderCache.h"
#include "DX9/DX9_PixelShaderCache.h"
#include "DX9/DX9_Render.h"
#include "DX9/DX9_VertexManager.h"

#endif

#include "OGL/OGL_TextureCache.h"
#include "OGL/OGL_VertexShaderCache.h"
#include "OGL/OGL_PixelShaderCache.h"
#include "OGL/OGL_Render.h"
#include "OGL/OGL_VertexManager.h"

#include "EmuWindow.h"

// TODO: ifdef wx this
#include "VideoConfigDiag.h"

#include "Main.h"

#define PLUGIN_NAME		"Dolphin Video Merge [broken]"
#if defined(DEBUGFAST)
#define PLUGIN_FULL_NAME	PLUGIN_NAME" (DebugFast)"
#elif defined(_DEBUG)
#define PLUGIN_FULL_NAME	PLUGIN_NAME" (Debug)"
#else
#define PLUGIN_FULL_NAME	PLUGIN_NAME
#endif

HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
PLUGIN_GLOBALS *g_globals = NULL;

const char* const g_gfxapi_names[] =
{
	"Software",
	"OpenGL",
	"Direct3D 9",
	"Direct3D 11",
};

#define INI_NAME	"gfx_new.ini"

// TODO: save to ini file
// TODO: move to VideoConfig or something
int g_gfxapi = 3;

// shits
RendererBase *g_renderer;
TextureCacheBase *g_texture_cache;
VertexManagerBase *g_vertex_manager;
VertexShaderCacheBase* g_vertex_shader_cache;
PixelShaderCacheBase* g_pixel_shader_cache;
FramebufferManagerBase* g_framebuffer_manager;

static bool s_PluginInitialized = false;
volatile u32 s_swapRequested = false;
static u32 s_efbAccessRequested = false;
static volatile u32 s_FifoShuttingDown = false;

int frameCount;

#if defined(HAVE_WX) && HAVE_WX
class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		wxSetInstance(hinstDLL);
		wxInitialize();
		break; 

	case DLL_PROCESS_DETACH:
		wxUninitialize();
		break;

	default:
		break;
	}

	g_hInstance = hinstDLL;
	return true;
}
#endif

static volatile struct
{
	u32 xfbAddr;
	FieldType field;
	u32 fbWidth;
	u32 fbHeight;
} s_beginFieldArgs;

static inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

// Run from the graphics thread (from Fifo.cpp)
void VideoFifo_CheckSwapRequest()
{
	if(g_ActiveConfig.bUseXFB)
	{
		if (Common::AtomicLoadAcquire(s_swapRequested))
		{
			EFBRectangle rc;
			g_renderer->Swap(s_beginFieldArgs.xfbAddr, s_beginFieldArgs.field,
				s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight, rc);
			Common::AtomicStoreRelease(s_swapRequested, false);
		}
	}
}

// Run from the graphics thread (from Fifo.cpp)
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	if (g_ActiveConfig.bUseXFB)
	{
		if(Common::AtomicLoadAcquire(s_swapRequested))
		{
			u32 aLower = xfbAddr;
			u32 aUpper = xfbAddr + 2 * fbWidth * fbHeight;
			u32 bLower = s_beginFieldArgs.xfbAddr;
			u32 bUpper = s_beginFieldArgs.xfbAddr + 2 * s_beginFieldArgs.fbWidth * s_beginFieldArgs.fbHeight;

			if (addrRangesOverlap(aLower, aUpper, bLower, bUpper))
				VideoFifo_CheckSwapRequest();
		}
	}
}

static struct
{
	EFBAccessType type;
	u32 x;
	u32 y;
	u32 Data;
} s_accessEFBArgs;

static u32 s_AccessEFBResult = 0;

void VideoFifo_CheckEFBAccess()
{
	if (Common::AtomicLoadAcquire(s_efbAccessRequested))
	{
		s_AccessEFBResult = g_renderer->AccessEFB(s_accessEFBArgs.type, s_accessEFBArgs.x, s_accessEFBArgs.y);

		Common::AtomicStoreRelease(s_efbAccessRequested, false);
	}
}

void VideoFifo_CheckAsyncRequest()
{
	VideoFifo_CheckSwapRequest();
	VideoFifo_CheckEFBAccess();
}

unsigned int Callback_PeekMessages()
{
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return 0;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 1;
}

void UpdateFPSDisplay(const char *text)
{
	char temp[512];
	sprintf_s(temp, 512, "SVN R%s: DX11: %s", svn_rev_str, text);
	SetWindowTextA(EmuWindow::GetWnd(), temp);
}

// GLOBAL I N T E R F A C E 
// ____________________________________________________________________________
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    A pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
void GetDllInfo(PLUGIN_INFO* _pPluginInfo)
{
	memcpy(_pPluginInfo->Name, PLUGIN_FULL_NAME, sizeof(PLUGIN_FULL_NAME));
	_pPluginInfo->Type = PLUGIN_TYPE_VIDEO;
	_pPluginInfo->Version = 0x0100;
}

// ___________________________________________________________________________
// Function: DllConfig
// Purpose:  This function is optional function that is provided
//           to allow the user to configure the DLL
// input:    A handle to the window that calls this function
// output:   none
//
void DllConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX

	VideoConfigDiag* const m_config_diag = new VideoConfigDiag((wxWindow *)_hParent);
	m_config_diag->ShowModal();
	m_config_diag->Destroy();

	g_Config.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + INI_NAME).c_str());

	SLEEP(50);	// hax to keep Dolphin window from staying hidden
#endif
}

// ___________________________________________________________________________
// Function: DllDebugger
// Purpose:  Open the debugger
// input:    a handle to the window that calls this function
// output:   none
//
void* DllDebugger(void *_hParent, bool Show)
{
	// TODO:
	return NULL;
}

// ___________________________________________________________________________
// Function: DllSetGlobals
// Purpose:  Set the pointer for globals variables
// input:    a pointer to the global struct
// output:   none
//
void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	g_globals = _pPluginGlobals;
}

// ___________________________________________________________________________
// Function: Initialize
// Purpose: Initialize the plugin
// input:    Init
// output:   none
//
void Initialize(void *init)
{
	frameCount = 0;

	g_VideoInitialize = *(SVideoInitialize*)init;

	InitXFBConvTables();

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + INI_NAME).c_str());
	g_Config.GameIniLoad(g_globals->game_ini);
	UpdateActiveConfig();

	g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Loading - Please wait."));
	if (NULL == g_VideoInitialize.pWindowHandle)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return;
	}

	g_VideoInitialize.pPeekMessages = Callback_PeekMessages;
	g_VideoInitialize.pUpdateFPSDisplay = UpdateFPSDisplay;

	*(SVideoInitialize*)init = g_VideoInitialize;

	//OSD::AddMessage("Dolphin ... Video Plugin", 5000);
	s_PluginInitialized = true;
}

// ___________________________________________________________________________
// Function: Shutdown
// Purpose:  This function is called when the emulator is shutting down
//           a game allowing the dll to de-initialise.
// input:    none
// output:   none
//
void Shutdown(void)
{
	s_efbAccessRequested = false;
	s_FifoShuttingDown = false;
	s_swapRequested = false;

	// VideoCommon
	DLCache::Shutdown();
	CommandProcessor::Shutdown();
	PixelShaderManager::Shutdown();
	VertexShaderManager::Shutdown();
	OpcodeDecoder_Shutdown();
	VertexLoaderManager::Shutdown();
	Fifo_Shutdown();

	// internal interfaces
	EmuWindow::Close();

	s_PluginInitialized = false;

	delete g_pixel_shader_cache;
	delete g_vertex_shader_cache;
	delete g_vertex_manager;
	delete g_texture_cache;
	delete g_renderer;
}

// ___________________________________________________________________________
// Function: DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
void DoState(unsigned char **ptr, int mode)
{
	PanicAlert("DoState");
}

// ___________________________________________________________________________
// Function: EmuStateChange
// Purpose: Notifies the plugin of a change in emulation state
// input:    newState
// output:   none
//
void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	Fifo_RunLoop(newState == PLUGIN_EMUSTATE_PLAY);
}

// I N T E R F A C E 


// __________________________________________________________________________________________________
// Function: Video_Prepare
// Purpose:  This function is called from the EmuThread before the
// 		     emulation has started. It is just for threadsensitive 
//   	     APIs like OpenGL.
// input:    none
// output:   none
//
void Video_Prepare(void)
{
	s_efbAccessRequested = false;
	s_FifoShuttingDown = false;
	s_swapRequested = false;

	switch (g_gfxapi)
	{
#ifdef _WIN32
	case GFXAPI_D3D9:
		g_renderer = new DX9::Renderer;
		g_texture_cache = new DX9::TextureCache;
		g_vertex_manager = new DX9::VertexManager;
		g_vertex_shader_cache = new DX9::VertexShaderCache;
		g_pixel_shader_cache = new DX9::PixelShaderCache;
		break;

	case GFXAPI_D3D11:
		g_renderer = new DX11::Renderer;
		g_texture_cache = new DX11::TextureCache;
		g_vertex_manager = new DX11::VertexManager;
		g_vertex_shader_cache = new DX11::VertexShaderCache;
		g_pixel_shader_cache = new DX11::PixelShaderCache;
		break;
#endif
	
	default:
	case GFXAPI_OPENGL:
		g_renderer = new OGL::Renderer;
		g_texture_cache = new OGL::TextureCache;
		g_vertex_manager = new OGL::VertexManager;
		g_vertex_shader_cache = new OGL::VertexShaderCache;
		g_pixel_shader_cache = new OGL::PixelShaderCache;
		break;
	}

	// VideoCommon
	BPInit();
	Fifo_Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	CommandProcessor::Init();
	PixelEngine::Init();
	DLCache::Init();

	// tell the host that the window is ready
	g_VideoInitialize.pCoreMessage(WM_USER_CREATE);
}

// __________________________________________________________________________________________________
// Function: Video_BeginField
// Purpose:  When a field begins in the VI emulator, this function tells the video plugin what the
//           parameters of the upcoming field are. The video plugin should make sure the previous
//           field is on the player's display before returning.
// input:    vi parameters of the upcoming field
// output:   none
//
void Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	if (s_PluginInitialized && g_ActiveConfig.bUseXFB)
	{
		if (g_VideoInitialize.bOnThread)
		{
			while (Common::AtomicLoadAcquire(s_swapRequested) && !s_FifoShuttingDown)
				//Common::SleepCurrentThread(1);
				Common::YieldCPU();
		}
		else
			VideoFifo_CheckSwapRequest();
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.field = field;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbHeight = fbHeight;

		Common::AtomicStoreRelease(s_swapRequested, true);
	}
}

// __________________________________________________________________________________________________
// Function: Video_EndField
// Purpose:  When a field ends in the VI emulator, this function notifies the video plugin. The video
//           has permission to swap the field to the player's display.
// input:    none
// output:   none
//
void Video_EndField()
{
	return;
}

// __________________________________________________________________________________________________
// Function: Video_AccessEFB
// input:    type of access (r/w, z/color, ...), x coord, y coord
// output:   response to the access request (ex: peek z data at specified coord)
//
u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	if (s_PluginInitialized)
	{
		s_accessEFBArgs.type = type;
		s_accessEFBArgs.x = x;
		s_accessEFBArgs.y = y;
		s_accessEFBArgs.Data = InputData;
		Common::AtomicStoreRelease(s_efbAccessRequested, true);

		if (g_VideoInitialize.bOnThread)
		{
			while (Common::AtomicLoadAcquire(s_efbAccessRequested) && !s_FifoShuttingDown)
				//Common::SleepCurrentThread(1);
				Common::YieldCPU();
		}
		else
			VideoFifo_CheckEFBAccess();

		return s_AccessEFBResult;
	}
	return 0;
}

// __________________________________________________________________________________________________
// Function: Video_Screenshot
// input:    Filename
// output:   true if all was okay
//
void Video_Screenshot(const char *_szFilename)
{
	PanicAlert("Screenshots are not yet supported.");
	return;
}

// __________________________________________________________________________________________________
// Function: Video_EnterLoop
// Purpose:  Enters the video fifo dispatch loop. This is only used in Dual Core mode.
// input:    none
// output:   none
//
void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

// __________________________________________________________________________________________________
// Function: Video_ExitLoop
// Purpose:  Exits the video dispatch loop. This is only used in Dual Core mode.
// input:    none
// output:   none
//
void Video_ExitLoop()
{
	Fifo_ExitLoop();
	s_FifoShuttingDown = true;
}

// __________________________________________________________________________________________________
// Function: Video_SetRendering
// Purpose:  Sets video rendering on and off. Currently used for frame skipping
// input:    Enabled toggle
// output:   none
//
void Video_SetRendering(bool bEnabled)
{
	PanicAlert("SetRendering is not yet supported.");
}

// __________________________________________________________________________________________________
// Function: Video_AddMessage
// Purpose:  Adds a message to the display queue, to be shown forthe specified time
// input:    pointer to the null-terminated string, time in milliseconds
// output:   none
//
void Video_AddMessage(const char* pstr, unsigned int milliseconds)
{
	return;
}

void Video_CommandProcessorRead16(u16& _rReturnValue, const u32 _Address)
{
	CommandProcessor::Read16(_rReturnValue, _Address);
}

void Video_CommandProcessorWrite16(const u16 _Data, const u32 _Address)
{
	CommandProcessor::Write16(_Data, _Address);
}

void Video_PixelEngineRead16(u16& _rReturnValue, const u32 _Address)
{
	PixelEngine::Read16(_rReturnValue, _Address);
}

void Video_PixelEngineWrite16(const u16 _Data, const u32 _Address)
{
	PixelEngine::Write16(_Data, _Address);
}

void Video_PixelEngineWrite32(const u32 _Data, const u32 _Address)
{
	PixelEngine::Write32(_Data, _Address);
}

void Video_GatherPipeBursted(void)
{
	CommandProcessor::GatherPipeBursted();
}

void Video_WaitForFrameFinish(void)
{
	CommandProcessor::WaitForFrameFinish();
}

// __________________________________________________________________________________________________
// Function: Video_IsFifoBusy
// Purpose:  Return if the FIFO is proecessing data, that is used for sync gfx thread and emulator
//           thread in CoreTiming
// input:    none
// output:   bool
//
bool Video_IsFifoBusy(void)
{
	return CommandProcessor::isFifoBusy;
}

void Video_AbortFrame(void)
{
	CommandProcessor::AbortFrame();
}
