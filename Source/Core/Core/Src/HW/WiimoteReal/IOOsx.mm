#import <CoreServices/CoreServices.h>
extern "C" OSErr UpdateSystemActivity(UInt8 activity);
#define BLUETOOTH_VERSION_USE_CURRENT
#import <IOBluetooth/IOBluetooth.h>

#include "Common.h"
#include "../Wiimote.h"
#include "WiimoteReal.h"

@interface SearchBT: NSObject
{
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

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (WiimoteReal::g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: WiimoteReal::g_wiimotes[i]->btd] == TRUE)
			wm = WiimoteReal::g_wiimotes[i];
	}
	if (wm == NULL)
	{
		WARN_LOG(WIIMOTE, "Received packet for unknown wiimote");
		return;
	}

	if (length > MAX_PAYLOAD)
	{
		WARN_LOG(WIIMOTE, "Dropping packet for wiimote %i, too large",
				wm->index + 1);
		return;
	}

	if (wm->queue[wm->writer].len != 0)
	{
		WARN_LOG(WIIMOTE, "Dropping packet for wiimote %i, queue full",
				wm->index + 1);
		return;
	}

	memcpy(wm->queue[wm->writer].data, data, length);
	wm->queue[wm->writer].len = length;

	wm->writer++;
	wm->outstanding++;
	if (wm->writer == QUEUE_SIZE)
		wm->writer = 0;

	if (wm->outstanding > wm->watermark)
	{
		wm->watermark = wm->outstanding;
		WARN_LOG(WIIMOTE, "New queue watermark %i for wiimote %i",
				wm->watermark, wm->index + 1);
	}

	CFRunLoopStop(CFRunLoopGetCurrent());

	(void)UpdateSystemActivity(1);
}

- (void) l2capChannelClosed: (IOBluetoothL2CAPChannel *) l2capChannel
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	WiimoteReal::Wiimote *wm = NULL;

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (WiimoteReal::g_wiimotes[i] == NULL)
			continue;
		if ([device isEqual: WiimoteReal::g_wiimotes[i]->btd] == TRUE)
			wm = WiimoteReal::g_wiimotes[i];
	}
	if (wm == NULL)
	{
		WARN_LOG(WIIMOTE, "Received packet for unknown wiimote");
		return;
	}

	WARN_LOG(WIIMOTE, "L2CAP channel was closed for wiimote %i", wm->index + 1);

	if (l2capChannel == wm->cchan)
		wm->cchan = nil;

	if (l2capChannel == wm->ichan)
		wm->ichan = nil;
}
@end

namespace WiimoteReal
{

// Find wiimotes.
// wm is an array of max_wiimotes wiimotes
// Returns the total number of found wiimotes.
int FindWiimotes(Wiimote** wm, int max_wiimotes)
{
	IOBluetoothHostController *bth;
	IOBluetoothDeviceInquiry *bti;
	SearchBT *sbt;
	NSEnumerator *en;
	int found_devices = 0, found_wiimotes = 0;

	// Count the number of already found wiimotes
	for (int i = 0; i < MAX_WIIMOTES; ++i)
	{
		if (wm[i])
			found_wiimotes++;
	}

	bth = [[IOBluetoothHostController alloc] init];
	if ([bth addressAsString] == nil)
	{
		WARN_LOG(WIIMOTE, "No bluetooth host controller");
		[bth release];
		return found_wiimotes;
	}

	sbt = [[SearchBT alloc] init];
	sbt->maxDevices = max_wiimotes;
	bti = [[IOBluetoothDeviceInquiry alloc] init];
	[bti setDelegate: sbt];
	[bti setInquiryLength: 5];
	[bti setSearchCriteria: kBluetoothServiceClassMajorAny
		majorDeviceClass: kBluetoothDeviceClassMajorPeripheral
		minorDeviceClass: kBluetoothDeviceClassMinorPeripheral2Joystick
		];
	[bti setUpdateNewDeviceNames: FALSE];

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
	for (int i = 0; (i < found_devices) && (found_wiimotes < max_wiimotes); i++)
	{
		IOBluetoothDevice *tmp_btd = [en nextObject];

		bool new_wiimote = true;
		// Determine if this wiimote has already been found.
		for (int j = 0; j < MAX_WIIMOTES && new_wiimote; ++j)
		{
			if (wm[j] && [tmp_btd isEqual: wm[j]->btd] == TRUE)
				new_wiimote = false;
		}

		if (new_wiimote)
		{
			// Find an unused slot
			unsigned int k = 0;
			for ( ; k < MAX_WIIMOTES &&
					!(WIIMOTE_SRC_REAL & g_wiimote_sources[k] && !wm[k]); ++k)
			{};
			wm[k] = new Wiimote(k);
			wm[k]->btd = tmp_btd;
			found_wiimotes++;
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

	if (IsConnected()) return false;

	[btd openL2CAPChannelSync: &cchan
		withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	[btd openL2CAPChannelSync: &ichan
		withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (ichan == NULL || cchan == NULL)
	{
		ERROR_LOG(WIIMOTE, "Unable to open L2CAP channels for wiimote %i",
				index + 1);
		RealDisconnect();
		return false;
	}

	NOTICE_LOG(WIIMOTE, "Connected to wiimote %i at %s",
			index + 1, [[btd getAddressString] UTF8String]);

	m_connected = true;

	Handshake();

	SetLEDs(WIIMOTE_LED_1 << index);

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

	[cchan closeChannel];
	[ichan closeChannel];
	[btd closeConnection];
}

unsigned char *Wiimote::IORead()
{
	int bytes;

	if (!IsConnected())
		return NULL;

	if (outstanding == 0)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);

	if (queue[reader].len == 0)
		return 0;

	bytes = queue[reader].len;
	unsigned char *buffer = new unsigned char[MAX_PAYLOAD];
	memcpy(buffer, queue[reader].data, bytes);
	queue[reader].len = 0;

	reader++;
	outstanding--;
	if (reader == QUEUE_SIZE)
		reader = 0;

	if (buffer[0] == '\0')
		bytes = 0;

	return buffer;
}

int Wiimote::IOWrite(unsigned char* buf, int len)
{
	IOReturn ret;

	ret = [cchan writeAsync: buf length: len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}

};
