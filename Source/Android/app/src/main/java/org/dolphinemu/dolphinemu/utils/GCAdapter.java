// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

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
import android.os.Build;
import android.widget.Toast;

import androidx.annotation.Keep;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.services.USBPermService;

import java.util.HashMap;
import java.util.Map;

public class GCAdapter
{
  public static UsbManager manager;

  @Keep
  static byte[] controllerPayload = new byte[37];

  static UsbDeviceConnection usbConnection;
  static UsbInterface usbInterface;
  static UsbEndpoint usbIn;
  static UsbEndpoint usbOut;

  private static void requestPermission()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
      {
        if (!manager.hasPermission(dev))
        {
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

  public static void shutdown()
  {
    usbConnection.close();
  }

  @Keep
  public static int getFd()
  {
    return usbConnection.getFileDescriptor();
  }

  @Keep
  public static boolean queryAdapter()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
      {
        if (manager.hasPermission(dev))
          return true;
        else
          requestPermission();
      }
    }
    return false;
  }

  public static void initAdapter()
  {
    byte[] init = {0x13};
    usbConnection.bulkTransfer(usbOut, init, init.length, 0);
  }

  @Keep
  public static int input()
  {
    return usbConnection.bulkTransfer(usbIn, controllerPayload, controllerPayload.length, 16);
  }

  @Keep
  public static int output(byte[] rumble)
  {
    return usbConnection.bulkTransfer(usbOut, rumble, 5, 16);
  }

  @Keep
  public static boolean openAdapter()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
      {
        if (manager.hasPermission(dev))
        {
          usbConnection = manager.openDevice(dev);

          Log.info("GCAdapter: Number of configurations: " + dev.getConfigurationCount());
          Log.info("GCAdapter: Number of interfaces: " + dev.getInterfaceCount());

          if (dev.getConfigurationCount() > 0 && dev.getInterfaceCount() > 0)
          {
            UsbConfiguration conf = dev.getConfiguration(0);
            usbInterface = conf.getInterface(0);
            usbConnection.claimInterface(usbInterface, true);

            Log.info("GCAdapter: Number of endpoints: " + usbInterface.getEndpointCount());

            if (usbInterface.getEndpointCount() == 2)
            {
              for (int i = 0; i < usbInterface.getEndpointCount(); ++i)
                if (usbInterface.getEndpoint(i).getDirection() == UsbConstants.USB_DIR_IN)
                  usbIn = usbInterface.getEndpoint(i);
                else
                  usbOut = usbInterface.getEndpoint(i);

              initAdapter();
              return true;
            }
            else
            {
              usbConnection.releaseInterface(usbInterface);
            }
          }

          Toast.makeText(DolphinApplication.getAppContext(), R.string.replug_gc_adapter,
                  Toast.LENGTH_LONG).show();
          usbConnection.close();
        }
      }
    }
    return false;
  }
}
