// SPDX-License-Identifier: GPL-2.0-or-later

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
import android.os.Build;

import androidx.annotation.Keep;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.services.USBPermService;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public class WiimoteAdapter
{
  final static int MAX_PAYLOAD = 23;
  final static int MAX_WIIMOTES = 4;
  final static int TIMEOUT = 200;
  final static short NINTENDO_VENDOR_ID = 0x057e;
  final static short NINTENDO_WIIMOTE_PRODUCT_ID = 0x0306;
  public static UsbManager manager;

  static UsbDeviceConnection usbConnection;
  static UsbInterface[] usbInterface = new UsbInterface[MAX_WIIMOTES];
  static UsbEndpoint[] usbIn = new UsbEndpoint[MAX_WIIMOTES];

  @Keep
  public static byte[][] wiimotePayload = new byte[MAX_WIIMOTES][MAX_PAYLOAD];

  private static void requestPermission()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID &&
              dev.getVendorId() == NINTENDO_VENDOR_ID)
      {
        if (!manager.hasPermission(dev))
        {
          Log.warning("Requesting permission for Wii Remote adapter");

          Context context = DolphinApplication.getAppContext();
          Intent intent = new Intent(context, USBPermService.class);

          int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ?
                  PendingIntent.FLAG_IMMUTABLE : 0;
          PendingIntent pendingIntent = PendingIntent.getService(context, 0, intent, flags);

          manager.requestPermission(dev, pendingIntent);
        }
      }
    }
  }

  @Keep
  public static boolean queryAdapter()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID &&
              dev.getVendorId() == NINTENDO_VENDOR_ID)
      {
        if (manager.hasPermission(dev))
          return true;
        else
          requestPermission();
      }
    }
    return false;
  }

  @Keep
  public static int input(int index)
  {
    return usbConnection.bulkTransfer(usbIn[index], wiimotePayload[index], MAX_PAYLOAD, TIMEOUT);
  }

  @Keep
  public static int output(int index, byte[] buf, int size)
  {
    byte report_number = buf[0];

    // Remove the report number from the buffer
    buf = Arrays.copyOfRange(buf, 1, buf.length);
    size--;

    final int LIBUSB_REQUEST_TYPE_CLASS = (1 << 5);
    final int LIBUSB_RECIPIENT_INTERFACE = 0x1;
    final int LIBUSB_ENDPOINT_OUT = 0;

    final int HID_SET_REPORT = 0x9;
    final int HID_OUTPUT = (2 << 8);

    int write = usbConnection.controlTransfer(
            LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
            HID_SET_REPORT,
            HID_OUTPUT | report_number,
            index,
            buf, size,
            1000);

    if (write < 0)
      return 0;

    return write + 1;
  }

  @Keep
  public static boolean openAdapter()
  {
    // If the adapter is already open. Don't attempt to do it again
    if (usbConnection != null && usbConnection.getFileDescriptor() != -1)
      return true;

    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == NINTENDO_WIIMOTE_PRODUCT_ID &&
              dev.getVendorId() == NINTENDO_VENDOR_ID)
      {
        if (manager.hasPermission(dev))
        {
          usbConnection = manager.openDevice(dev);
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
              usbInterface[i] = dev.getInterface(i);
              usbConnection.claimInterface(usbInterface[i], true);

              // One endpoint per Wii Remote. Input only
              // Output reports go through the control channel.
              usbIn[i] = usbInterface[i].getEndpoint(0);
              Log.info("Interface " + i + " endpoint count:" + usbInterface[i].getEndpointCount());
            }
            return true;
          }
          else
          {
            // XXX: Message that the device was found, but it needs to be unplugged and plugged back in?
            usbConnection.close();
          }
        }
      }
    }
    return false;
  }
}
