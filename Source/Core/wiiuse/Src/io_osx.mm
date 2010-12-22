/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header$
 *
 */

/**
 *	@file
 *	@brief Handles device I/O for OS X.
 */

#import <CoreServices/CoreServices.h>
extern "C" OSErr UpdateSystemActivity(UInt8 activity);
#define BLUETOOTH_VERSION_USE_CURRENT
#import <IOBluetooth/IOBluetooth.h>

#include "Common.h"
#include "HW/Wiimote.h"
#include "wiiuse_internal.h"

static int wiiuse_connect_single(struct wiimote_t *wm, char *address);

static struct wiimote_t *motes[MAX_WIIMOTES];

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
	data: (byte *) data
	length: (NSUInteger) length
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	struct wiimote_t *wm = NULL;

	for (int i = 0; i < MAX_WIIMOTES; i++) {
		if (motes[i] == NULL)
			continue;
		if ([device isEqual: motes[i]->btd] == TRUE)
			wm = motes[i];
		if (i == MAX_WIIMOTES && wm == NULL) {
			WARN_LOG(WIIMOTE, "Received packet for unknown wiimote");
			return;
		}
	}

	if (length > MAX_PAYLOAD) {
		WARN_LOG(WIIMOTE, "Dropping wiimote packet, too large [id %i]",
			wm->unid);
		return;
	}

	if (wm->queue[wm->writer].len != 0) {
		WARN_LOG(WIIMOTE, "Dropping wiimote packet, queue full [id %i]",
			wm->unid);
		return;
	}

	memcpy(wm->queue[wm->writer].data, data, length);
	wm->queue[wm->writer].len = length;

	wm->writer++;
	wm->outstanding++;
	if (wm->writer == QUEUE_SIZE)
		wm->writer = 0;

	if (wm->outstanding > wm->watermark) {
		wm->watermark = wm->outstanding;
		WARN_LOG(WIIMOTE, "New wiimote queue watermark %i [id %i]",
			wm->watermark, wm->unid);
	}

	CFRunLoopStop(CFRunLoopGetCurrent());

	(void)UpdateSystemActivity(1);
}

- (void) l2capChannelClosed: (IOBluetoothL2CAPChannel *) l2capChannel
{
	IOBluetoothDevice *device = [l2capChannel getDevice];
	struct wiimote_t *wm = NULL;

	for (int i = 0; i < MAX_WIIMOTES; i++) {
		if (motes[i] == NULL)
			continue;
		if ([device isEqual: motes[i]->btd] == TRUE)
			wm = motes[i];
		if (i == MAX_WIIMOTES && wm == NULL) {
			WARN_LOG(WIIMOTE, "Received packet for unknown wiimote");
			return;
		}
	}

	WARN_LOG(WIIMOTE, "L2CAP channel was closed [id %i]", wm->unid);

	if (l2capChannel == wm->cchan)
		wm->cchan = nil;

	if (l2capChannel == wm->ichan)
		wm->ichan = nil;
}
@end

/**
 *	@brief Find a wiimote or wiimotes.
 *
 *	@param wm		An array of wiimote_t structures.
 *	@param max_wiimotes	The number of wiimote structures in \a wm.
 *	@param timeout		The number of seconds before timing out.
 *
 *	@return The number of wiimotes found.
 *
 *	@see wiiuse_connect()
 *
 *	This function will only look for wiimote devices.
 *	When a device is found the address in the structures will be set.
 *	You can then call wiiuse_connect() to connect to the found
 *	devices.
 */
int wiiuse_find(struct wiimote_t **wm, int max_wiimotes, int timeout)
{
	IOBluetoothHostController *bth;
	IOBluetoothDeviceInquiry *bti;
	SearchBT *sbt;
	NSEnumerator *en;
	int i, found_devices = 0;

	if (max_wiimotes > MAX_WIIMOTES)
		return 0;

	bth = [[IOBluetoothHostController alloc] init];
	if ([bth addressAsString] == nil)
	{
		WARN_LOG(WIIMOTE, "No bluetooth host controller");
		[bth release];
		return 0;
	}

	sbt = [[SearchBT alloc] init];
	sbt->maxDevices = max_wiimotes;
	bti = [[IOBluetoothDeviceInquiry alloc] init];
	[bti setDelegate: sbt];
	[bti setInquiryLength: timeout];
	[bti setSearchCriteria: kBluetoothServiceClassMajorAny
		majorDeviceClass: kBluetoothDeviceClassMajorPeripheral
		minorDeviceClass: kBluetoothDeviceClassMinorPeripheral2Joystick
		];
	[bti setUpdateNewDeviceNames: FALSE];

	IOReturn ret = [bti start];
	if (ret == kIOReturnSuccess)
		[bti retain];
	else
		ERROR_LOG(WIIMOTE, "Unable to do bluetooth discovery");

	CFRunLoopRun();

	[bti stop];
	found_devices = [[bti foundDevices] count];

	NOTICE_LOG(WIIMOTE, "Found %i bluetooth device%c", found_devices,
		found_devices == 1 ? '\0' : 's');

	en = [[bti foundDevices] objectEnumerator];
	for (i = 0; i < found_devices; i++) {
		wm[i]->btd = [en nextObject];
		WIIMOTE_ENABLE_STATE(wm[i], WIIMOTE_STATE_DEV_FOUND);
		motes[i] = wm[i];
	}
	for (i = found_devices; i < MAX_WIIMOTES; i++)
		motes[i] = NULL;

	[bth release];
	[bti release];
	[sbt release];

	return found_devices;
}

/**
 *	@brief Connect to a wiimote or wiimotes once an address is known.
 *
 *	@param wm		An array of wiimote_t structures.
 *	@param wiimotes		The number of wiimote structures in \a wm.
 *
 *	@return The number of wiimotes that successfully connected.
 *
 *	@see wiiuse_find()
 *	@see wiiuse_connect_single()
 *	@see wiiuse_disconnect()
 *
 *	Connect to a number of wiimotes when the address is already set
 *	in the wiimote_t structures.  These addresses are normally set
 *	by the wiiuse_find() function, but can also be set manually.
 */
int wiiuse_connect(struct wiimote_t **wm, int wiimotes)
{
	int i, connected = 0;

	for (i = 0; i < wiimotes; ++i)
	{
		if (!WIIMOTE_IS_SET(wm[i], WIIMOTE_STATE_DEV_FOUND))
			continue;

		if (wiiuse_connect_single(wm[i], NULL))
			connected++;
	}

	return connected;
}

/**
 *	@brief Connect to a wiimote with a known address.
 *
 *	@param wm	Pointer to a wiimote_t structure.
 *	@param address	The address of the device to connect to. If NULL,
 *			use the address in the struct set by wiiuse_find().
 *
 *	@return 1 on success, 0 on failure
 */
static int wiiuse_connect_single(struct wiimote_t *wm, char *address)
{
	ConnectBT *cbt = [[ConnectBT alloc] init];

	if (wm == NULL || WIIMOTE_IS_CONNECTED(wm))
		return 0;

	[wm->btd openL2CAPChannelSync: &wm->cchan
		withPSM: kBluetoothL2CAPPSMHIDControl delegate: cbt];
	[wm->btd openL2CAPChannelSync: &wm->ichan
		withPSM: kBluetoothL2CAPPSMHIDInterrupt delegate: cbt];
	if (wm->ichan == NULL || wm->cchan == NULL) {
		ERROR_LOG(WIIMOTE, "Unable to open L2CAP channels [id %i]",
			wm->unid);
		wiiuse_disconnect(wm);
	}

	NOTICE_LOG(WIIMOTE, "Connected to wiimote at %s [id %i]",
		[[wm->btd getAddressString] UTF8String], wm->unid);

	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
	wiiuse_set_report_type(wm);

	[cbt release];

	return 1;
}

/**
 *	@brief Disconnect a wiimote.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@see wiiuse_connect()
 *
 *	Note that this will not free the wiimote structure.
 */
void wiiuse_disconnect(struct wiimote_t *wm)
{

	if (wm == NULL)
		return;

	NOTICE_LOG(WIIMOTE, "Disconnecting wiimote [id %i]", wm->unid);

	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);

	[wm->cchan closeChannel];
	[wm->ichan closeChannel];
	[wm->btd closeConnection];
}

int wiiuse_io_read(struct wiimote_t *wm)
{
	int bytes;

	if (!WIIMOTE_IS_CONNECTED(wm))
		return 0;

	if (wm->outstanding == 0)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);

	if (wm->queue[wm->reader].len == 0)
		return 0;

	bytes = wm->queue[wm->reader].len;
	memcpy(wm->event_buf, wm->queue[wm->reader].data, bytes);
	wm->queue[wm->reader].len = 0;

	wm->reader++;
	wm->outstanding--;
	if (wm->reader == QUEUE_SIZE)
		wm->reader = 0;

	if (wm->event_buf[0] == '\0')
		bytes = 0;
	
	return bytes;
}

int wiiuse_io_write(struct wiimote_t *wm, byte *buf, int len)
{
	IOReturn ret;

	ret = [wm->cchan writeAsync: buf length: len refcon: nil];

	if (ret == kIOReturnSuccess)
		return len;
	else
		return 0;
}
