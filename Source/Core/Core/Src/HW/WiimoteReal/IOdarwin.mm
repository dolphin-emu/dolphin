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

	for (int i = 0; i < MAX_WIIMOTES; i++) {
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

	for (int i = 0; i < MAX_WIIMOTES; i++) {
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

	wm->RealDisconnect();
}
@end

namespace WiimoteReal
{

// Find wiimotes.
// wm is an array of max_wiimotes wiimotes
// Returns the total number of found wiimotes.
int FindWiimotes(Wiimote **wm, int max_wiimotes)
{
	IOBluetoothHostController *bth;
	IOBluetoothDeviceInquiry *bti;
	SearchBT *sbt;
	NSEnumerator *en;
	int found_devices = 0, found_wiimotes = 0;

	// Count the number of already found wiimotes
	for (int i = 0; i < MAX_WIIMOTES; ++i)
		if (wm[i])
			found_wiimotes++;

	bth = [[IOBluetoothHostController alloc] init];
	if ([bth addressAsString] == nil) {
		WARN_LOG(WIIMOTE, "No bluetooth host controller");
		[bth release];
		return found_wiimotes;
	}

	sbt = [[SearchBT alloc] init];
	sbt->maxDevices = max_wiimotes - found_wiimotes;
	bti = [[IOBluetoothDeviceInquiry alloc] init];
	[bti setDelegate: sbt];
	[bti setInquiryLength: 5];

	if ([bti start] == kIOReturnSuccess)
		[bti retain];
	else
		ERROR_LOG(WIIMOTE, "Unable to do bluetooth discovery");

	CFRunLoopRun();

	[bti stop];
	found_devices = [[bti foundDevices] count];

	NOTICE_LOG(WIIMOTE, "Found %i bluetooth device%c", found_devices,
		found_devices == 1 ? '\0' : 's');

	en = [[bti foundDevices] objectEnumerator];
	for (int i = 0; i < found_devices; i++)
	{
		IOBluetoothDevice *dev = [en nextObject];
		if (!IsValidBluetoothName([[dev name] UTF8String]))
			continue;
		// Find an unused slot
		for (int k = 0; k < MAX_WIIMOTES; k++) {
			if (wm[k] != NULL ||
				!(g_wiimote_sources[k] & WIIMOTE_SRC_REAL))
				continue;

			wm[k] = new Wiimote(k);
			wm[k]->btd = dev;
			found_wiimotes++;
			break;
		}
	}

	[bth release];
	[bti release];
	[sbt release];

	return found_wiimotes;
}

// Connect to a wiimote with a known address.
bool Wiimote::Connect()
{
	ConnectBT *cbt = [[ConnectBT alloc] init];

	if (IsConnected())
		return false;

	[btd openL2CAPChannelSync: &cchan
		withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	[btd openL2CAPChannelSync: &ichan
		withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (ichan == NULL || cchan == NULL) {
		ERROR_LOG(WIIMOTE, "Unable to open L2CAP channels "
			"for wiimote %i", index + 1);
		RealDisconnect();
		return false;
	}
    
    // As of 10.8 these need explicit retaining or writing to the wiimote has a very high
    // chance of crashing and burning.
    [ichan retain];
    [cchan retain];

	NOTICE_LOG(WIIMOTE, "Connected to wiimote %i at %s",
		index + 1, [[btd getAddressString] UTF8String]);

	m_connected = true;

	Handshake();
	SetLEDs(WIIMOTE_LED_1 << index);

	m_wiimote_thread = std::thread(std::mem_fun(&Wiimote::ThreadFunc), this);

	[cbt release];

	return true;
}

// Disconnect a wiimote.
void Wiimote::RealDisconnect()
{
	if (!IsConnected())
		return;

	NOTICE_LOG(WIIMOTE, "Disconnecting wiimote %i", index + 1);

	m_connected = false;

	if (m_wiimote_thread.joinable())
		m_wiimote_thread.join();

	[btd closeConnection];
    [ichan release];
    [cchan release];
	btd = NULL;
	cchan = NULL;
	ichan = NULL;
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

int Wiimote::IOWrite(unsigned char *buf, int len)
{
	IOReturn ret;

	if (!IsConnected())
		return 0;

	ret = [ichan writeAsync: buf length: len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}

};
