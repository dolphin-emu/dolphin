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
import android.widget.Toast;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.services.USBPermService;

import java.util.HashMap;
import java.util.Map;

public class Java_GCAdapter
{
  public static UsbManager manager;
  static byte[] controller_payload = new byte[37];

  static UsbDeviceConnection usb_con;
  static UsbInterface usb_intf;
  static UsbEndpoint usb_in;
  static UsbEndpoint usb_out;

  private static void RequestPermission()
  {
    Context context = EmulationActivity.get();
    if (context != null)
    {
      HashMap<String, UsbDevice> devices = manager.getDeviceList();
      for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
      {
        UsbDevice dev = pair.getValue();
        if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
        {
          if (!manager.hasPermission(dev))
          {
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
      Log.warning("Cannot request GameCube Adapter permission as EmulationActivity is null.");
    }

  }

  public static void Shutdown()
  {
    usb_con.close();
  }

  public static int GetFD()
  {
    return usb_con.getFileDescriptor();
  }

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

  public static int Input()
  {
    return usb_con.bulkTransfer(usb_in, controller_payload, controller_payload.length, 16);
  }

  public static int Output(byte[] rumble)
  {
    return usb_con.bulkTransfer(usb_out, rumble, 5, 16);
  }

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

          final Activity emulationActivity = EmulationActivity.get();
          if (emulationActivity != null)
          {
            emulationActivity.runOnUiThread(() -> Toast.makeText(emulationActivity,
              "GameCube Adapter couldn't be opened. Please re-plug the device.",
              Toast.LENGTH_LONG).show());
          }
          else
          {
            Log.warning("Cannot show toast for GameCube Adapter failure.");
          }
          usb_con.close();
        }
      }
    }
    return false;
  }
}
