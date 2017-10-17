package org.dolphinemu.dolphinemu.utils;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbConfiguration;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.services.USBPermService;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public class Java_WiimoteAdapter
{
	final static int MAX_PAYLOAD = 23;
	final static int MAX_WIIMOTES = 4;
	final static int TIMEOUT = 200;
	final static short NINTENDO_VENDOR_ID = 0x057e;
	final static short NINTENDO_WIIMOTE_PRODUCT_ID = 0x0306;
	public static UsbManager manager;

	static UsbDeviceConnection usb_con;
	static UsbInterface[] usb_intf = new UsbInterface[MAX_WIIMOTES];
	static UsbEndpoint[] usb_in = new UsbEndpoint[MAX_WIIMOTES];

	public static byte[][] wiimote_payload = new byte[MAX_WIIMOTES][MAX_PAYLOAD];

	private static void RequestPermission()
	{
		Context context = NativeLibrary.sEmulationActivity.get();
		if (context != null)
		{
			HashMap<String, UsbDevice> devices = manager.getDeviceList();
			for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
			{
				UsbDevice dev = pair.getValue();
				if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID && dev.getVendorId() == NINTENDO_VENDOR_ID)
				{
					if (!manager.hasPermission(dev))
					{
						Log.warning("Requesting permission for Wii Remote adapter");
						Intent intent = new Intent();
						PendingIntent pend_intent;
						intent.setClass(context, USBPermService.class);
						pend_intent = PendingIntent.getService(context, 0, intent, 0);
						manager.requestPermission(dev, pend_intent);
					}
				}
			}
		}
		else
		{
			Log.warning("Cannot request Wiimote adapter permission as EmulationActivity is null.");
		}
	}

	public static boolean QueryAdapter()
	{
		HashMap<String, UsbDevice> devices = manager.getDeviceList();
		for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
		{
			UsbDevice dev = pair.getValue();
			if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID && dev.getVendorId() == NINTENDO_VENDOR_ID)
			{
				if (manager.hasPermission(dev))
					return true;
				else
					RequestPermission();
			}
		}
		return false;
	}

	public static int Input(int index)
	{
		return usb_con.bulkTransfer(usb_in[index], wiimote_payload[index], MAX_PAYLOAD, TIMEOUT);
	}

	public static int Output(int index, byte[] buf, int size)
	{
		byte report_number = buf[0];

		// Remove the report number from the buffer
		buf = Arrays.copyOfRange(buf, 1, buf.length);
		size--;

		final int LIBUSB_REQUEST_TYPE_CLASS  = (1 << 5);
		final int LIBUSB_RECIPIENT_INTERFACE = 0x1;
		final int LIBUSB_ENDPOINT_OUT        = 0;

		final int HID_SET_REPORT = 0x9;
		final int HID_OUTPUT     = (2 << 8);

		int write = usb_con.controlTransfer(
				LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
				HID_SET_REPORT,
				HID_OUTPUT | report_number,
				index,
				buf, size,
				1000);

		if (write < 0)
			return -1;

		return write + 1;
	}

	public static boolean OpenAdapter()
	{
		// If the adapter is already open. Don't attempt to do it again
		if (usb_con != null && usb_con.getFileDescriptor() != -1)
			return true;

		HashMap<String, UsbDevice> devices = manager.getDeviceList();
		for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
		{
			UsbDevice dev = pair.getValue();
			if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID && dev.getVendorId() == NINTENDO_VENDOR_ID)
			{
				if (manager.hasPermission(dev))
				{
					usb_con = manager.openDevice(dev);
					UsbConfiguration conf = dev.getConfiguration(0);

					Log.info("Number of configurations: " + dev.getConfigurationCount());
					Log.info("Number of Interfaces: " + dev.getInterfaceCount());
					Log.info("Number of Interfaces from conf: " + conf.getInterfaceCount());

					// Sometimes the interface count is returned as zero.
					// Means the device needs to be unplugged and plugged back in again
					if (dev.getInterfaceCount() > 0)
					{
						for (int i = 0; i < MAX_WIIMOTES; ++i)
						{
							// One interface per Wii Remote
							usb_intf[i] = dev.getInterface(i);
							usb_con.claimInterface(usb_intf[i], true);

							// One endpoint per Wii Remote. Input only
							// Output reports go through the control channel.
							usb_in[i] = usb_intf[i].getEndpoint(0);
							Log.info("Interface " + i + " endpoint count:" + usb_intf[i].getEndpointCount());
						}
						return true;
					}
					else
					{
						// XXX: Message that the device was found, but it needs to be unplugged and plugged back in?
						usb_con.close();
					}
				}
			}
		}
		return false;
	}
}
