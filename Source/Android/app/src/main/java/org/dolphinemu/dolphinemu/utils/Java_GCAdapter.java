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

public class Java_GCAdapter
{
  public static UsbManager manager;

  @Keep
  static byte[] controller_payload = new byte[37];

  static UsbDeviceConnection usb_con;
  static UsbInterface usb_intf;
  static UsbEndpoint usb_in;
  static UsbEndpoint usb_out;

  private static void RequestPermission()
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

  public static void Shutdown()
  {
    usb_con.close();
  }

  @Keep
  public static int GetFD()
  {
    return usb_con.getFileDescriptor();
  }

  @Keep
  public static boolean QueryAdapter()
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
          RequestPermission();
      }
    }
    return false;
  }

  public static void InitAdapter()
  {
    byte[] init = {0x13};
    usb_con.bulkTransfer(usb_out, init, init.length, 0);
  }

  @Keep
  public static int Input()
  {
    return usb_con.bulkTransfer(usb_in, controller_payload, controller_payload.length, 16);
  }

  @Keep
  public static int Output(byte[] rumble)
  {
    return usb_con.bulkTransfer(usb_out, rumble, 5, 16);
  }

  @Keep
  public static boolean OpenAdapter()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
      {
        if (manager.hasPermission(dev))
        {
          usb_con = manager.openDevice(dev);

          Log.info("GCAdapter: Number of configurations: " + dev.getConfigurationCount());
          Log.info("GCAdapter: Number of interfaces: " + dev.getInterfaceCount());

          if (dev.getConfigurationCount() > 0 && dev.getInterfaceCount() > 0)
          {
            UsbConfiguration conf = dev.getConfiguration(0);
            usb_intf = conf.getInterface(0);
            usb_con.claimInterface(usb_intf, true);

            Log.info("GCAdapter: Number of endpoints: " + usb_intf.getEndpointCount());

            if (usb_intf.getEndpointCount() == 2)
            {
              for (int i = 0; i < usb_intf.getEndpointCount(); ++i)
                if (usb_intf.getEndpoint(i).getDirection() == UsbConstants.USB_DIR_IN)
                  usb_in = usb_intf.getEndpoint(i);
                else
                  usb_out = usb_intf.getEndpoint(i);

              InitAdapter();
              return true;
            }
            else
            {
              usb_con.releaseInterface(usb_intf);
            }
          }

          Toast.makeText(DolphinApplication.getAppContext(), R.string.replug_gc_adapter,
                  Toast.LENGTH_LONG).show();
          usb_con.close();
        }
      }
    }
    return false;
  }
}
