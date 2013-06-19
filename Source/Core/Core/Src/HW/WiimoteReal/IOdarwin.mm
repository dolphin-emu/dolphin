#define BLUETOOTH_VERSION_USE_CURRENT
#import <IOBluetooth/IOBluetooth.h>

#include "Common.h"
#include "WiimoteReal.h"

@interface SearchBT: NSObject {
@public
	unsigned int maxDevices;
}
@end

@implementation SearchBT
- (void) deviceInquiryComplete: (IOBluetoothDeviceInquiry *) sender
	error: (IOReturn) error
	aborted: (BOOL) aborted
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void) deviceInquiryDeviceFound: (IOBluetoothDeviceInquiry *) sender
	device: (IOBluetoothDevice *) device
{
	NOTICE_LOG(WIIMOTE, "Discovered bluetooth device at %s: %s",
		[[device getAddressString] UTF8String],
		[[device getName] UTF8String]);

	if ([[sender foundDevices] count] == maxDevices)
		[sender stop];
}
@end

@interface ConnectBT: NSObject {}
@end

@implementation ConnectBT
- (void) l2capChannelData: (IOBluetoothL2CAPChannel *) l2capChannel
	data: (unsigned char *) data
	length: (NSUInteger) length
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	WiimoteReal::Wiimote *wm = NULL;
	
	std::lock_guard<std::recursive_mutex> lk(WiimoteReal::g_refresh_lock);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (WiimoteReal::g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: WiimoteReal::g_wiimotes[i]->btd] == TRUE)
			wm = WiimoteReal::g_wiimotes[i];
	}

	if (wm == NULL) {
		ERROR_LOG(WIIMOTE, "Received packet for unknown wiimote");
		return;
	}

	if (length > MAX_PAYLOAD) {
		WARN_LOG(WIIMOTE, "Dropping packet for wiimote %i, too large",
				wm->index + 1);
		return;
	}

	if (wm->inputlen != 0) {
		WARN_LOG(WIIMOTE, "Dropping packet for wiimote %i, queue full",
				wm->index + 1);
		return;
	}

	memcpy(wm->input, data, length);
	wm->inputlen = length;

	(void)wm->Read();

	(void)UpdateSystemActivity(UsrActivity);
}

- (void) l2capChannelClosed: (IOBluetoothL2CAPChannel *) l2capChannel
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	WiimoteReal::Wiimote *wm = NULL;
	
	std::lock_guard<std::recursive_mutex> lk(WiimoteReal::g_refresh_lock);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (WiimoteReal::g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: WiimoteReal::g_wiimotes[i]->btd] == TRUE)
			wm = WiimoteReal::g_wiimotes[i];
	}

	if (wm == NULL) {
		ERROR_LOG(WIIMOTE, "Channel for unknown wiimote was closed");
		return;
	}

	WARN_LOG(WIIMOTE, "Lost channel to wiimote %i", wm->index + 1);

	wm->Disconnect();
}
@end

namespace WiimoteReal
{

WiimoteScanner::WiimoteScanner()
	: m_run_thread()
	, m_want_wiimotes()
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
	SearchBT *sbt;
	NSEnumerator *en;
	found_board = NULL;

	bth = [[IOBluetoothHostController alloc] init];
	if ([bth addressAsString] == nil)
	{
		WARN_LOG(WIIMOTE, "No bluetooth host controller");
		[bth release];
		return;
	}

	sbt = [[SearchBT alloc] init];
	sbt->maxDevices = 32;
	bti = [[IOBluetoothDeviceInquiry alloc] init];
	[bti setDelegate: sbt];
	[bti setInquiryLength: 2];

	if ([bti start] == kIOReturnSuccess)
		[bti retain];
	else
		ERROR_LOG(WIIMOTE, "Unable to do bluetooth discovery");

	CFRunLoopRun();

	[bti stop];
	int found_devices = [[bti foundDevices] count];

	if (found_devices)
		NOTICE_LOG(WIIMOTE, "Found %i bluetooth devices", found_devices);

	en = [[bti foundDevices] objectEnumerator];
	for (int i = 0; i < found_devices; i++)
	{
		IOBluetoothDevice *dev = [en nextObject];
		if (!IsValidBluetoothName([[dev name] UTF8String]))
			continue;

		Wiimote *wm = new Wiimote();
		wm->btd = dev;
		
		if(IsBalanceBoardName([[dev name] UTF8String]))
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

bool WiimoteScanner::IsReady() const
{
	// TODO: only return true when a BT device is present
	return true;
}

// Connect to a wiimote with a known address.
bool Wiimote::Connect()
{
	if (IsConnected())
		return false;

	ConnectBT *cbt = [[ConnectBT alloc] init];

	[btd openL2CAPChannelSync: &cchan
		withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	[btd openL2CAPChannelSync: &ichan
		withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (ichan == NULL || cchan == NULL)
	{
		ERROR_LOG(WIIMOTE, "Unable to open L2CAP channels "
			"for wiimote %i", index + 1);
		Disconnect();
		
		[cbt release];
		return false;
	}
    
    // As of 10.8 these need explicit retaining or writing to the wiimote has a very high
    // chance of crashing and burning.
    [ichan retain];
    [cchan retain];

	NOTICE_LOG(WIIMOTE, "Connected to wiimote %i at %s",
		index + 1, [[btd getAddressString] UTF8String]);

	m_connected = true;

	[cbt release];
	return true;
}

// Disconnect a wiimote.
void Wiimote::Disconnect()
{
	if (btd != NULL)
		[btd closeConnection];

	if (ichan != NULL)
		[ichan release];

	if (cchan != NULL)
		[cchan release];

	btd = NULL;
	cchan = NULL;
	ichan = NULL;

	if (!IsConnected())
		return;

	NOTICE_LOG(WIIMOTE, "Disconnecting wiimote %i", index + 1);

	m_connected = false;
}

bool Wiimote::IsConnected() const
{
	return m_connected;
}

int Wiimote::IORead(unsigned char *buf)
{
	int bytes;

	if (!IsConnected())
		return 0;

	bytes = inputlen;
	memcpy(buf, input, bytes);
	inputlen = 0;

	return bytes;
}

int Wiimote::IOWrite(const unsigned char *buf, int len)
{
	IOReturn ret;

	if (!IsConnected())
		return 0;

	ret = [ichan writeAsync: const_cast<void*>((void *)buf) length: len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}

};
