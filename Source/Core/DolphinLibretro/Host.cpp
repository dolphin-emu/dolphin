
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#ifdef __ANDROID__
#include "Core/HW/WiimoteReal/IOAndroid.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"
#endif

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

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

#ifdef __ANDROID__

namespace GCAdapter
{
void SetAdapterCallback(std::function<void(void)> func)
{
}

void Init()
{
}

void StartScanThread()
{
}

void StopScanThread()
{
}

void Shutdown()
{
}

GCPadStatus Input(int chan)
{
  return {};
}

bool DeviceConnected(int chan)
{
  return false;
}

bool UseAdapter()
{
  return false;
}

void ResetRumble()
{
}

void Output(int chan, u8 rumble_command)
{
}

bool IsDetected()
{
  return false;
}

bool IsDriverDetected()
{
  return false;
}
}  // namespace GCAdapter

namespace WiimoteReal
{
void WiimoteScannerAndroid::FindWiimotes(std::vector<Wiimote*>& found_wiimotes,
                                         Wiimote*& found_board)
{
}

WiimoteAndroid::WiimoteAndroid(int index) : Wiimote(), m_mayflash_index(index)
{
}

WiimoteAndroid::~WiimoteAndroid()
{
}

bool WiimoteAndroid::ConnectInternal()
{
  return false;
}

void WiimoteAndroid::DisconnectInternal()
{
}

bool WiimoteAndroid::IsConnected() const
{
  return false;
}

int WiimoteAndroid::IORead(u8* buf)
{
  return 0;
}

int WiimoteAndroid::IOWrite(u8 const* buf, size_t len)
{
  return len;
}

void InitAdapterClass()
{
}
}  // namespace WiimoteReal

namespace ButtonManager
{
bool GetButtonPressed(int padID, ButtonType button)
{
  return false;
}
float GetAxisValue(int padID, ButtonType axis)
{
  return 0;
}
}  // namespace ButtonManager
#endif
