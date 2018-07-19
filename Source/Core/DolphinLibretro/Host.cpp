
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Host.h"

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

void Host_Message(HostMessageID id)
{
  DEBUG_LOG(COMMON, "message id: %i\n", (int)id);
}

void* Host_GetRenderHandle()
{
  return (void*)-1;
}

void Host_UpdateTitle(const std::string& title)
{
#if 0
  DEBUG_LOG(COMMON, "title : %s\n", title.c_str());
#endif
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_RendererHasFocus()
{
  /* called on input poll */
  return true;
}

bool Host_RendererIsFullscreen()
{
  return false;
}

void Host_ShowVideoConfig(void*, const std::string&)
{
}

void Host_YieldToUI()
{
}

bool Host_UINeedsControllerState()
{
  return false;
}
void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
}
