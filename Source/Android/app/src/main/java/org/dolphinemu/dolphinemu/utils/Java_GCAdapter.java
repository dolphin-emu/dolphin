package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbConfiguration;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;

import org.dolphinemu.dolphinemu.services.USBPermService;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class Java_GCAdapter {
	public static UsbManager manager;
	public static Activity our_activity;
	static byte[] controller_payload = new byte[37];
	static byte HasRead;

	static UsbDeviceConnection usb_con;
	static UsbInterface usb_intf;
	static UsbEndpoint usb_in;
	static UsbEndpoint usb_out;

	private static void RequestPermission()
	{
		HashMap<String, UsbDevice> devices = manager.getDeviceList();
		Iterator it = devices.entrySet().iterator();
		for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
		{
			UsbDevice dev = (UsbDevice) pair.getValue();
			if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
			{
				if (!manager.hasPermission(dev))
				{
					Intent intent = new Intent();
					PendingIntent pend_intent;
					intent.setClass(our_activity, USBPermService.class);
					pend_intent = PendingIntent.getService(our_activity, 0, intent, 0);
					manager.requestPermission(dev, pend_intent);
				}
			}
		}
	}

	public static void Shutdown()
	{
		usb_con.close();
	}
	public static int GetFD() { return usb_con.getFileDescriptor(); }

	public static boolean QueryAdapter()
	{
		HashMap<String, UsbDevice> devices = manager.getDeviceList();
		Iterator it = devices.entrySet().iterator();
		while (it.hasNext())
		{
			HashMap.Entry pair = (HashMap.Entry) it.next();
			UsbDevice dev = (UsbDevice) pair.getValue();
			if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
				if (manager.hasPermission(dev))
					return true;
				else
					RequestPermission();
		}
		return false;
	}

	public static void InitAdapter()
	{
		byte[] init = { 0x13 };
		usb_con.bulkTransfer(usb_in, init, init.length, 0);
	}

	public static int Input() {
		int read = usb_con.bulkTransfer(usb_in, controller_payload, controller_payload.length, 16);
		return read;
	}

	public static int Output(byte[] rumble) {
		int size = usb_con.bulkTransfer(usb_out, rumble, 5, 16);
		return size;
	}

	public static void OpenAdapter()
	{
		HashMap<String, UsbDevice> devices = manager.getDeviceList();
		Iterator it = devices.entrySet().iterator();
		while (it.hasNext())
		{
			HashMap.Entry pair = (HashMap.Entry)it.next();
			UsbDevice dev = (UsbDevice)pair.getValue();
			if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e) {
				if (manager.hasPermission(dev))
				{
					usb_con = manager.openDevice(dev);
					UsbConfiguration conf = dev.getConfiguration(0);
					usb_intf = conf.getInterface(0);
					usb_con.claimInterface(usb_intf, true);
					for (int i = 0; i < usb_intf.getEndpointCount(); ++i)
						if (usb_intf.getEndpoint(i).getDirection() == UsbConstants.USB_DIR_IN)
							usb_in = usb_intf.getEndpoint(i);
						else
							usb_out = usb_intf.getEndpoint(i);

					InitAdapter();
					return;
				}
			}

		}
	}
}
