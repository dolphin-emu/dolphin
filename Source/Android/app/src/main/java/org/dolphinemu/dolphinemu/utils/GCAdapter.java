// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
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
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;

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

  private static final String ACTION_GC_ADAPTER_PERMISSION_GRANTED =
          BuildConfig.APPLICATION_ID + ".GC_ADAPTER_PERMISSION_GRANTED";

  private static final Object hotplugCallbackLock = new Object();
  private static boolean hotplugCallbackEnabled = false;
  private static UsbDevice adapterDevice = null;
  private static BroadcastReceiver hotplugBroadcastReceiver = new BroadcastReceiver()
  {
    @Override
    public void onReceive(Context context, Intent intent)
    {
      onUsbDevicesChanged();
    }
  };

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

          int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ?
                  PendingIntent.FLAG_IMMUTABLE : 0;
          PendingIntent pendingIntent = PendingIntent.getBroadcast(context, 0,
                  new Intent(ACTION_GC_ADAPTER_PERMISSION_GRANTED), flags);

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
  public static boolean isUsbDeviceAvailable()
  {
    synchronized (hotplugCallbackLock)
    {
      return adapterDevice != null;
    }
  }

  @Nullable
  private static UsbDevice queryAdapter()
  {
    HashMap<String, UsbDevice> devices = manager.getDeviceList();
    for (Map.Entry<String, UsbDevice> pair : devices.entrySet())
    {
      UsbDevice dev = pair.getValue();
      if (dev.getProductId() == 0x0337 && dev.getVendorId() == 0x057e)
      {
        if (manager.hasPermission(dev))
          return dev;
        else
          requestPermission();
      }
    }
    return null;
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
    UsbDevice dev;
    synchronized (hotplugCallbackLock)
    {
      dev = adapterDevice;
    }
    if (dev == null)
    {
      return false;
    }

    usbConnection = manager.openDevice(dev);
    if (usbConnection == null)
    {
      return false;
    }

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
    return false;
  }

  @Keep
  public static void enableHotplugCallback()
  {
    synchronized (hotplugCallbackLock)
    {
      if (hotplugCallbackEnabled)
      {
        throw new IllegalStateException("enableHotplugCallback was called when already enabled");
      }

      IntentFilter filter = new IntentFilter();
      filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
      filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
      filter.addAction(ACTION_GC_ADAPTER_PERMISSION_GRANTED);

      ContextCompat.registerReceiver(DolphinApplication.getAppContext(), hotplugBroadcastReceiver,
              filter, ContextCompat.RECEIVER_EXPORTED);

      hotplugCallbackEnabled = true;

      onUsbDevicesChanged();
    }
  }

  @Keep
  public static void disableHotplugCallback()
  {
    synchronized (hotplugCallbackLock)
    {
      if (hotplugCallbackEnabled)
      {
        DolphinApplication.getAppContext().unregisterReceiver(hotplugBroadcastReceiver);
        hotplugCallbackEnabled = false;
        adapterDevice = null;
      }
    }
  }

  public static void onUsbDevicesChanged()
  {
    synchronized (hotplugCallbackLock)
    {
      if (adapterDevice != null)
      {
        boolean adapterStillConnected = manager.getDeviceList().entrySet().stream()
                .anyMatch(pair -> pair.getValue().getDeviceId() == adapterDevice.getDeviceId());

        if (!adapterStillConnected)
        {
          adapterDevice = null;
          onAdapterDisconnected();
        }
      }

      if (adapterDevice == null)
      {
        UsbDevice newAdapter = queryAdapter();
        if (newAdapter != null)
        {
          adapterDevice = newAdapter;
          onAdapterConnected();
        }
      }
    }
  }

  private static native void onAdapterConnected();

  private static native void onAdapterDisconnected();
}
