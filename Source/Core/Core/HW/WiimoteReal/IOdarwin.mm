#define BLUETOOTH_VERSION_USE_CURRENT

#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

@interface SearchBT: NSObject {
@public
	unsigned int maxDevices;
	bool done;
}
@end

@interface ConnectBT: NSObject {}
@end

namespace WiimoteReal
{

class WiimoteDarwin final : public Wiimote
{
public:
	WiimoteDarwin(IOBluetoothDevice* device);
	~WiimoteDarwin() override;

	// These are not protected/private because ConnectBT needs them.
	void DisconnectInternal() override;
	IOBluetoothDevice* m_btd;
	unsigned char* m_input;
	int m_inputlen;

protected:
	bool ConnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;
	void EnablePowerAssertionInternal() override;
	void DisablePowerAssertionInternal() override;

private:
	IOBluetoothL2CAPChannel* m_ichan;
	IOBluetoothL2CAPChannel* m_cchan;
	bool m_connected;
	CFRunLoopRef m_wiimote_thread_run_loop;
	IOPMAssertionID m_pm_assertion;
};

class WiimoteDarwinHid final : public Wiimote
{
public:
	WiimoteDarwinHid(IOHIDDeviceRef device);
	~WiimoteDarwinHid() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	static void ReportCallback(void* context, IOReturn result, void* sender, IOHIDReportType type, u32 reportID, u8* report, CFIndex reportLength);
	static void RemoveCallback(void* context, IOReturn result, void* sender);
	void QueueBufferReport(int length);
	IOHIDDeviceRef m_device;
	bool m_connected;
	std::atomic<bool> m_interrupted;
	Report m_report_buffer;
	Common::FifoQueue<Report> m_buffered_reports;
};

WiimoteScanner::WiimoteScanner()
{}

WiimoteScanner::~WiimoteScanner()
{}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	// TODO: find the device in the constructor and save it for later
	IOBluetoothHostController *bth;
	IOBluetoothDeviceInquiry *bti;
	IOHIDManagerRef hid;
	SearchBT *sbt;
	NSEnumerator *en;
	found_board = nullptr;

	bool btFailed = false;
	bool hidFailed = false;

	bth = [[IOBluetoothHostController alloc] init];
	btFailed = [bth addressAsString] == nil;
	if (btFailed)
		WARN_LOG(WIIMOTE, "No Bluetooth host controller");

	hid = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
	hidFailed = CFGetTypeID(hid) != IOHIDManagerGetTypeID();
	if (hidFailed)
		WARN_LOG(WIIMOTE, "No HID manager");

	if (hidFailed && btFailed)
	{
		CFRelease(hid);
		[bth release];
		return;
	}

	if (!btFailed)
	{
		sbt = [[SearchBT alloc] init];
		sbt->maxDevices = 32;
		bti = [[IOBluetoothDeviceInquiry alloc] init];
		[bti setDelegate: sbt];
		[bti setInquiryLength: 2];

		if ([bti start] != kIOReturnSuccess)
		{
			ERROR_LOG(WIIMOTE, "Unable to do Bluetooth discovery");
			[bth release];
			[sbt release];
			btFailed = true;
		}

		do
		{
			CFRunLoopRun();
		}
		while (!sbt->done);

		int found_devices = [[bti foundDevices] count];

		if (found_devices)
			NOTICE_LOG(WIIMOTE, "Found %i Bluetooth devices", found_devices);

		en = [[bti foundDevices] objectEnumerator];
		for (int i = 0; i < found_devices; i++)
		{
			IOBluetoothDevice *dev = [en nextObject];
			if (!IsValidBluetoothName([[dev name] UTF8String]))
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

	if (!hidFailed)
	{
		NSArray *criteria = @[
			@{ @kIOHIDVendorIDKey: @0x057e, @kIOHIDProductIDKey: @0x0306 },
			@{ @kIOHIDVendorIDKey: @0x057e, @kIOHIDProductIDKey: @0x0330 },
		];
		IOHIDManagerSetDeviceMatchingMultiple(hid, (CFArrayRef)criteria);
		if (IOHIDManagerOpen(hid, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
			NSLog(@"Failed to open HID Manager");
		CFSetRef devices = IOHIDManagerCopyDevices(hid);
		if (devices)
		{
			int found_devices = CFSetGetCount(devices);
			if (found_devices)
			{
				NOTICE_LOG(WIIMOTE, "Found %i HID devices", found_devices);

				IOHIDDeviceRef values[found_devices];
				CFSetGetValues(devices, reinterpret_cast<const void**>(&values));
				for (int i = 0; i < found_devices; i++)
				{
					Wiimote* wm = new  WiimoteDarwinHid(values[i]);
					found_wiimotes.push_back(wm);
				}
			}
		}
		CFRelease(hid);
	}
}

bool WiimoteScanner::IsReady() const
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

	ConnectBT *cbt = [[ConnectBT alloc] init];

	m_cchan = m_ichan = nil;

	IOReturn ret = [m_btd openConnection];
	if (ret)
	{
		ERROR_LOG(WIIMOTE, "Unable to open Bluetooth connection to Wiimote %i: %x",
		          m_index + 1, ret);
		[cbt release];
		return false;
	}

	ret = [m_btd openL2CAPChannelSync: &m_cchan
	           withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	if (ret)
	{
		ERROR_LOG(WIIMOTE, "Unable to open control channel for Wiimote %i: %x",
		          m_index + 1, ret);
		goto bad;
	}
	// Apple docs claim:
	// "The L2CAP channel object is already retained when this function returns
	// success; the channel must be released when the caller is done with it."
	// But without this, the channels get over-autoreleased, even though the
	// refcounting behavior here is clearly correct.
	[m_cchan retain];

	ret = [m_btd openL2CAPChannelSync: &m_ichan
	           withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (ret)
	{
		WARN_LOG(WIIMOTE, "Unable to open interrupt channel for Wiimote %i: %x",
		         m_index + 1, ret);
		goto bad;
	}
	[m_ichan retain];

	NOTICE_LOG(WIIMOTE, "Connected to Wiimote %i at %s",
	           m_index + 1, [[m_btd addressString] UTF8String]);

	m_connected = true;

	[cbt release];

	m_wiimote_thread_run_loop = (CFRunLoopRef) CFRetain(CFRunLoopGetCurrent());

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

int WiimoteDarwin::IORead(unsigned char *buf)
{
	m_input = buf;
	m_inputlen = -1;

	CFRunLoopRun();

	return m_inputlen;
}

int WiimoteDarwin::IOWrite(const unsigned char *buf, size_t len)
{
	IOReturn ret;

	if (!IsConnected())
		return 0;

	ret = [m_ichan writeAsync: const_cast<void*>((void *)buf) length: (int)len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}

void WiimoteDarwin::EnablePowerAssertionInternal()
{
	if (m_pm_assertion == kIOPMNullAssertionID)
	{
		if (IOReturn ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Dolphin Wiimote activity"), &m_pm_assertion))
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

WiimoteDarwinHid::WiimoteDarwinHid(IOHIDDeviceRef device) : m_device(device)
{
	CFRetain(m_device);
	m_connected = false;
	m_report_buffer = Report(MAX_PAYLOAD);
}

WiimoteDarwinHid::~WiimoteDarwinHid()
{
	Shutdown();
	CFRelease(m_device);
}

bool WiimoteDarwinHid::ConnectInternal()
{
	IOReturn ret = IOHIDDeviceOpen(m_device, kIOHIDOptionsTypeNone);
	m_connected = ret == kIOReturnSuccess;
	if (m_connected)
	{
		IOHIDDeviceScheduleWithRunLoop(m_device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		IOHIDDeviceRegisterInputReportCallback(m_device,
		                                       m_report_buffer.data() + 1,
		                                       MAX_PAYLOAD - 1,
		                                       &WiimoteDarwinHid::ReportCallback,
		                                       this);
		IOHIDDeviceRegisterRemovalCallback(m_device, &WiimoteDarwinHid::RemoveCallback, this);
		NOTICE_LOG(WIIMOTE, "Connected to Wiimote %i", m_index + 1);
	}
	else
	{
		ERROR_LOG(WIIMOTE, "Could not open IOHID Wiimote: %08x", ret);
	}

	return m_connected;
}

void WiimoteDarwinHid::DisconnectInternal()
{
	IOHIDDeviceUnscheduleFromRunLoop(m_device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOWakeup();
	IOReturn ret = IOHIDDeviceClose(m_device, kIOHIDOptionsTypeNone);
	if (ret != kIOReturnSuccess)
		ERROR_LOG(WIIMOTE, "Error closing IOHID Wiimote: %08x", ret);

	if (!IsConnected())
		return;

	NOTICE_LOG(WIIMOTE, "Disconnecting Wiimote %i", m_index + 1);

	m_buffered_reports.Clear();

	m_connected = false;
}

bool WiimoteDarwinHid::IsConnected() const
{
	return m_connected;
}

void WiimoteDarwinHid::IOWakeup()
{
	m_interrupted.store(true);
	CFRunLoopStop(CFRunLoopGetCurrent());
}

int WiimoteDarwinHid::IORead(u8* buf)
{
	Report rpt;
	m_interrupted.store(false);
	while (m_buffered_reports.Empty() && !m_interrupted.load())
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);

	if (m_buffered_reports.Pop(rpt))
	{
		memcpy(buf, rpt.data(), rpt.size());
		return rpt.size();
	}
	return -1;
}

int WiimoteDarwinHid::IOWrite(u8 const* buf, size_t len)
{
	IOReturn ret = IOHIDDeviceSetReport(m_device, kIOHIDReportTypeOutput, buf[1], buf + 1, len - 1);
	if (ret != kIOReturnSuccess)
	{
		ERROR_LOG(WIIMOTE, "Error writing to Wiimote: %08x", ret);
		return 0;
	}
	return len;
}

void WiimoteDarwinHid::QueueBufferReport(int length)
{
	Report rpt(m_report_buffer);
	rpt[0] = 0xa1;
	rpt.resize(length + 1);
	m_buffered_reports.Push(std::move(rpt));
}

void WiimoteDarwinHid::ReportCallback(void* context, IOReturn result, void*, IOHIDReportType type, u32 report_id, u8* report, CFIndex report_length)
{
	WiimoteDarwinHid* wm = static_cast<WiimoteDarwinHid*>(context);
	report[0] = report_id;
	wm->QueueBufferReport(report_length);
}

void WiimoteDarwinHid::RemoveCallback(void* context, IOReturn result, void*)
{
	WiimoteDarwinHid* wm = static_cast<WiimoteDarwinHid*>(context);
	wm->DisconnectInternal();
}

} // namespace

@implementation SearchBT
- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry *) sender
	error: (IOReturn) error
	aborted: (BOOL) aborted
{
	done = true;
	CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry *) sender
	device: (IOBluetoothDevice *) device
{
	NOTICE_LOG(WIIMOTE, "Discovered Bluetooth device at %s: %s",
		[[device addressString] UTF8String],
		[[device name] UTF8String]);

	if ([[sender foundDevices] count] == maxDevices)
		[sender stop];
}
@end

@implementation ConnectBT
- (void) l2capChannelData: (IOBluetoothL2CAPChannel *) l2capChannel
	data: (unsigned char *) data
	length: (NSUInteger) length
{
	IOBluetoothDevice *device = [l2capChannel device];
	WiimoteReal::WiimoteDarwin *wm = nullptr;

	std::lock_guard<std::recursive_mutex> lk(WiimoteReal::g_refresh_lock);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		wm = static_cast<WiimoteReal::WiimoteDarwin*>(WiimoteReal::g_wiimotes[i]);
		if (!wm)
			continue;
		if ([device isEqual: wm->m_btd])
			break;
		wm = nullptr;
	}

	if (wm == nullptr) {
		ERROR_LOG(WIIMOTE, "Received packet for unknown Wiimote");
		return;
	}

	if (length > MAX_PAYLOAD) {
		WARN_LOG(WIIMOTE, "Dropping packet for Wiimote %i, too large",
				wm->GetIndex() + 1);
		return;
	}

	if (wm->m_inputlen != -1) {
		WARN_LOG(WIIMOTE, "Dropping packet for Wiimote %i, queue full",
				wm->GetIndex() + 1);
		return;
	}

	memcpy(wm->m_input, data, length);
	wm->m_inputlen = length;

	CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void) l2capChannelClosed: (IOBluetoothL2CAPChannel *) l2capChannel
{
	IOBluetoothDevice *device = [l2capChannel device];
	WiimoteReal::WiimoteDarwin *wm = nullptr;

	std::lock_guard<std::recursive_mutex> lk(WiimoteReal::g_refresh_lock);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		wm = static_cast<WiimoteReal::WiimoteDarwin*>(WiimoteReal::g_wiimotes[i]);
		if (!wm)
			continue;
		if ([device isEqual: wm->m_btd])
			break;
		wm = nullptr;
	}

	if (wm == nullptr) {
		ERROR_LOG(WIIMOTE, "Channel for unknown Wiimote was closed");
		return;
	}

	WARN_LOG(WIIMOTE, "Lost channel to Wiimote %i", wm->GetIndex() + 1);

	wm->DisconnectInternal();
}
@end
