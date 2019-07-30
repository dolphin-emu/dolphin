#define BLUETOOTH_VERSION_USE_CURRENT

#include "Core/HW/WiimoteReal/IOdarwin.h"
#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteReal/IOdarwin_private.h"

@interface SearchBT : NSObject
{
@public
  unsigned int maxDevices;
  bool done;
}
@end

@interface ConnectBT : NSObject
{
}
@end

namespace WiimoteReal
{
WiimoteScannerDarwin::~WiimoteScannerDarwin()
{
  stopScanning = true;
}

void WiimoteScannerDarwin::FindWiimotes(std::vector<Wiimote*>& found_wiimotes,
                                        Wiimote*& found_board)
{
  // TODO: find the device in the constructor and save it for later
  IOBluetoothHostController* bth;
  IOBluetoothDeviceInquiry* bti;
  found_board = nullptr;

  bth = [[IOBluetoothHostController alloc] init];
  bool btFailed = [bth addressAsString] == nil;
  if (btFailed)
  {
    WARN_LOG(WIIMOTE, "No Bluetooth host controller");
    [bth release];
    return;
  }

  SearchBT* sbt = [[SearchBT alloc] init];
  sbt->maxDevices = 32;
  bti = [[IOBluetoothDeviceInquiry alloc] init];
  [bti setDelegate:sbt];
  [bti setInquiryLength:2];

  if ([bti start] != kIOReturnSuccess)
  {
    ERROR_LOG(WIIMOTE, "Unable to do Bluetooth discovery");
    [bth release];
    [sbt release];
    btFailed = true;
  }

  do
  {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
  } while (!sbt->done && !stopScanning);

  int found_devices = [[bti foundDevices] count];

  if (found_devices)
    NOTICE_LOG(WIIMOTE, "Found %i Bluetooth devices", found_devices);

  NSEnumerator* en = [[bti foundDevices] objectEnumerator];
  for (int i = 0; i < found_devices; i++)
  {
    IOBluetoothDevice* dev = [en nextObject];
    if (!IsValidDeviceName([[dev name] UTF8String]))
      continue;

    Wiimote* wm = new WiimoteDarwin([dev retain]);

    if (IsBalanceBoardName([[dev name] UTF8String]))
    {
      found_board = wm;
    }
    else
    {
      found_wiimotes.push_back(wm);
    }
  }

  [bth release];
  [bti release];
  [sbt release];
}

bool WiimoteScannerDarwin::IsReady() const
{
  // TODO: only return true when a BT device is present
  return true;
}

WiimoteDarwin::WiimoteDarwin(IOBluetoothDevice* device) : m_btd(device)
{
  m_inputlen = 0;
  m_connected = false;
  m_wiimote_thread_run_loop = nullptr;
  m_pm_assertion = kIOPMNullAssertionID;
}

WiimoteDarwin::~WiimoteDarwin()
{
  Shutdown();
  if (m_wiimote_thread_run_loop)
  {
    CFRelease(m_wiimote_thread_run_loop);
    m_wiimote_thread_run_loop = nullptr;
  }
  [m_btd release];
  m_btd = nil;
  DisablePowerAssertionInternal();
}

// Connect to a Wiimote with a known address.
bool WiimoteDarwin::ConnectInternal()
{
  if (IsConnected())
    return false;

  ConnectBT* cbt = [[ConnectBT alloc] init];

  m_cchan = m_ichan = nil;

  IOReturn ret = [m_btd openConnection];
  if (ret)
  {
    ERROR_LOG(WIIMOTE, "Unable to open Bluetooth connection to Wiimote %i: %x", m_index + 1, ret);
    [cbt release];
    return false;
  }

  ret = [m_btd openL2CAPChannelSync:&m_cchan withPSM:kBluetoothL2CAPPSMHIDControl delegate:cbt];
  if (ret)
  {
    ERROR_LOG(WIIMOTE, "Unable to open control channel for Wiimote %i: %x", m_index + 1, ret);
    goto bad;
  }
  // Apple docs claim:
  // "The L2CAP channel object is already retained when this function returns
  // success; the channel must be released when the caller is done with it."
  // But without this, the channels get over-autoreleased, even though the
  // refcounting behavior here is clearly correct.
  [m_cchan retain];

  ret = [m_btd openL2CAPChannelSync:&m_ichan withPSM:kBluetoothL2CAPPSMHIDInterrupt delegate:cbt];
  if (ret)
  {
    WARN_LOG(WIIMOTE, "Unable to open interrupt channel for Wiimote %i: %x", m_index + 1, ret);
    goto bad;
  }
  [m_ichan retain];

  NOTICE_LOG(WIIMOTE, "Connected to Wiimote %i at %s", m_index + 1,
             [[m_btd addressString] UTF8String]);

  m_connected = true;

  [cbt release];

  m_wiimote_thread_run_loop = (CFRunLoopRef)CFRetain(CFRunLoopGetCurrent());

  return true;

bad:
  DisconnectInternal();
  [cbt release];
  return false;
}

// Disconnect a Wiimote.
void WiimoteDarwin::DisconnectInternal()
{
  [m_ichan closeChannel];
  [m_ichan release];
  m_ichan = nil;

  [m_cchan closeChannel];
  [m_cchan release];
  m_cchan = nil;

  [m_btd closeConnection];

  if (!IsConnected())
    return;

  NOTICE_LOG(WIIMOTE, "Disconnecting Wiimote %i", m_index + 1);

  m_connected = false;
}

bool WiimoteDarwin::IsConnected() const
{
  return m_connected;
}

void WiimoteDarwin::IOWakeup()
{
  if (m_wiimote_thread_run_loop)
  {
    CFRunLoopStop(m_wiimote_thread_run_loop);
  }
}

int WiimoteDarwin::IORead(unsigned char* buf)
{
  m_input = buf;
  m_inputlen = -1;

  CFRunLoopRun();

  return m_inputlen;
}

int WiimoteDarwin::IOWrite(const unsigned char* buf, size_t len)
{
  IOReturn ret;

  if (!IsConnected())
    return 0;

  ret = [m_ichan writeAsync:const_cast<void*>((void*)buf) length:(int)len refcon:nil];

  if (ret == kIOReturnSuccess)
    return len;
  else
    return 0;
}

void WiimoteDarwin::EnablePowerAssertionInternal()
{
  if (m_pm_assertion == kIOPMNullAssertionID)
  {
    if (IOReturn ret = IOPMAssertionCreateWithName(
            kIOPMAssertPreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
            CFSTR("Dolphin Wiimote activity"), &m_pm_assertion))
      ERROR_LOG(WIIMOTE, "Could not create power management assertion: %08x", ret);
  }
}

void WiimoteDarwin::DisablePowerAssertionInternal()
{
  if (m_pm_assertion != kIOPMNullAssertionID)
  {
    if (IOReturn ret = IOPMAssertionRelease(m_pm_assertion))
      ERROR_LOG(WIIMOTE, "Could not release power management assertion: %08x", ret);
  }
}
}  // namespace

@implementation SearchBT
- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                        error:(IOReturn)error
                      aborted:(BOOL)aborted {
  done = true;
}

- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                          device:(IOBluetoothDevice*)device {
  NOTICE_LOG(WIIMOTE, "Discovered Bluetooth device at %s: %s", [[device addressString] UTF8String],
             [[device name] UTF8String]);

  if ([[sender foundDevices] count] == maxDevices)
    [sender stop];
}
@end

@implementation ConnectBT
- (void)l2capChannelData:(IOBluetoothL2CAPChannel*)l2capChannel
                    data:(unsigned char*)data
                  length:(NSUInteger)length {
  IOBluetoothDevice* device = [l2capChannel device];
  WiimoteReal::WiimoteDarwin* wm = nullptr;

  std::lock_guard<std::mutex> lk(WiimoteReal::g_wiimotes_mutex);

  for (int i = 0; i < MAX_WIIMOTES; i++)
  {
    wm = static_cast<WiimoteReal::WiimoteDarwin*>(WiimoteReal::g_wiimotes[i].get());
    if (!wm)
      continue;
    if ([device isEqual:wm->m_btd])
      break;
    wm = nullptr;
  }

  if (wm == nullptr)
  {
    ERROR_LOG(WIIMOTE, "Received packet for unknown Wiimote");
    return;
  }

  if (length > WiimoteCommon::MAX_PAYLOAD)
  {
    WARN_LOG(WIIMOTE, "Dropping packet for Wiimote %i, too large", wm->GetIndex() + 1);
    return;
  }

  if (wm->m_inputlen != -1)
  {
    WARN_LOG(WIIMOTE, "Dropping packet for Wiimote %i, queue full", wm->GetIndex() + 1);
    return;
  }

  memcpy(wm->m_input, data, length);
  wm->m_inputlen = length;

  CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void)l2capChannelClosed:(IOBluetoothL2CAPChannel*)l2capChannel {
  IOBluetoothDevice* device = [l2capChannel device];
  WiimoteReal::WiimoteDarwin* wm = nullptr;

  std::lock_guard<std::mutex> lk(WiimoteReal::g_wiimotes_mutex);

  for (int i = 0; i < MAX_WIIMOTES; i++)
  {
    wm = static_cast<WiimoteReal::WiimoteDarwin*>(WiimoteReal::g_wiimotes[i].get());
    if (!wm)
      continue;
    if ([device isEqual:wm->m_btd])
      break;
    wm = nullptr;
  }

  if (wm == nullptr)
  {
    ERROR_LOG(WIIMOTE, "Channel for unknown Wiimote was closed");
    return;
  }

  WARN_LOG(WIIMOTE, "Lost channel to Wiimote %i", wm->GetIndex() + 1);

  wm->DisconnectInternal();
}
@end
