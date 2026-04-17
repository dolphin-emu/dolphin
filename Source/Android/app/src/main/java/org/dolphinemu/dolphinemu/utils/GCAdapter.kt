// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbConfiguration
import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.widget.Toast
import androidx.annotation.Keep
import androidx.core.content.ContextCompat
import org.dolphinemu.dolphinemu.BuildConfig
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R

object GCAdapter {
    lateinit var manager: UsbManager

    @Keep
    @JvmField
    var controllerPayload = ByteArray(37)

    private var usbConnection: UsbDeviceConnection? = null
    private var usbInterface: UsbInterface? = null
    private var usbIn: UsbEndpoint? = null
    private var usbOut: UsbEndpoint? = null

    private val ACTION_GC_ADAPTER_PERMISSION_GRANTED =
        BuildConfig.APPLICATION_ID + ".GC_ADAPTER_PERMISSION_GRANTED"

    private val hotplugCallbackLock = Any()
    private var hotplugCallbackEnabled = false
    private var adapterDevice: UsbDevice? = null
    private val hotplugBroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            onUsbDevicesChanged()
        }
    }

    private fun requestPermission() {
        val devices = manager.deviceList
        for (dev in devices.values) {
            if (dev.productId == 0x0337 && dev.vendorId == 0x057e) {
                if (!manager.hasPermission(dev)) {
                    val context = DolphinApplication.getAppContext()

                    val pendingIntent = PendingIntent.getBroadcast(
                        context,
                        0,
                        Intent(ACTION_GC_ADAPTER_PERMISSION_GRANTED),
                        PendingIntent.FLAG_IMMUTABLE
                    )

                    manager.requestPermission(dev, pendingIntent)
                }
            }
        }
    }

    @JvmStatic
    fun shutdown() {
        usbConnection!!.close()
    }

    @Keep
    @JvmStatic
    fun getFd(): Int {
        return usbConnection!!.fileDescriptor
    }

    @Keep
    @JvmStatic
    fun isUsbDeviceAvailable(): Boolean {
        synchronized(hotplugCallbackLock) {
            return adapterDevice != null
        }
    }

    private fun queryAdapter(): UsbDevice? {
        val devices = manager.deviceList
        for (dev in devices.values) {
            if (dev.productId == 0x0337 && dev.vendorId == 0x057e) {
                if (manager.hasPermission(dev)) {
                    return dev
                } else {
                    requestPermission()
                }
            }
        }
        return null
    }

    @JvmStatic
    fun initAdapter() {
        val init = byteArrayOf(0x13)
        usbConnection!!.bulkTransfer(usbOut, init, init.size, 0)
    }

    @Keep
    @JvmStatic
    fun input(): Int {
        return usbConnection!!.bulkTransfer(usbIn, controllerPayload, controllerPayload.size, 16)
    }

    @Keep
    @JvmStatic
    fun output(rumble: ByteArray): Int {
        return usbConnection!!.bulkTransfer(usbOut, rumble, 5, 16)
    }

    @Keep
    @JvmStatic
    fun openAdapter(): Boolean {
        val dev: UsbDevice?
        synchronized(hotplugCallbackLock) {
            dev = adapterDevice
        }
        if (dev == null) {
            return false
        }

        usbConnection = manager.openDevice(dev)
        if (usbConnection == null) {
            return false
        }

        Log.info("GCAdapter: Number of configurations: " + dev.configurationCount)
        Log.info("GCAdapter: Number of interfaces: " + dev.interfaceCount)

        if (dev.configurationCount > 0 && dev.interfaceCount > 0) {
            val conf: UsbConfiguration = dev.getConfiguration(0)
            usbInterface = conf.getInterface(0)
            usbConnection!!.claimInterface(usbInterface, true)

            Log.info("GCAdapter: Number of endpoints: " + usbInterface!!.endpointCount)

            if (usbInterface!!.endpointCount == 2) {
                for (i in 0 until usbInterface!!.endpointCount) {
                    if (usbInterface!!.getEndpoint(i).direction == UsbConstants.USB_DIR_IN) {
                        usbIn = usbInterface!!.getEndpoint(i)
                    } else {
                        usbOut = usbInterface!!.getEndpoint(i)
                    }
                }

                initAdapter()
                return true
            } else {
                usbConnection!!.releaseInterface(usbInterface)
            }
        }

        Toast.makeText(
            DolphinApplication.getAppContext(), R.string.replug_gc_adapter, Toast.LENGTH_LONG
        ).show()
        usbConnection!!.close()
        return false
    }

    @Keep
    @JvmStatic
    fun enableHotplugCallback() {
        synchronized(hotplugCallbackLock) {
            if (hotplugCallbackEnabled) {
                throw IllegalStateException("enableHotplugCallback was called when already enabled")
            }

            val filter = IntentFilter()
            filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            filter.addAction(ACTION_GC_ADAPTER_PERMISSION_GRANTED)

            ContextCompat.registerReceiver(
                DolphinApplication.getAppContext(),
                hotplugBroadcastReceiver,
                filter,
                ContextCompat.RECEIVER_EXPORTED
            )

            hotplugCallbackEnabled = true

            onUsbDevicesChanged()
        }
    }

    @Keep
    @JvmStatic
    fun disableHotplugCallback() {
        synchronized(hotplugCallbackLock) {
            if (hotplugCallbackEnabled) {
                DolphinApplication.getAppContext().unregisterReceiver(hotplugBroadcastReceiver)
                hotplugCallbackEnabled = false
                adapterDevice = null
            }
        }
    }

    @JvmStatic
    fun onUsbDevicesChanged() {
        synchronized(hotplugCallbackLock) {
            if (adapterDevice != null) {
                val adapterStillConnected = manager.deviceList.values.any {
                    it.deviceId == adapterDevice!!.deviceId
                }

                if (!adapterStillConnected) {
                    adapterDevice = null
                    onAdapterDisconnected()
                }
            }

            if (adapterDevice == null) {
                val newAdapter = queryAdapter()
                if (newAdapter != null) {
                    adapterDevice = newAdapter
                    onAdapterConnected()
                }
            }
        }
    }

    @JvmStatic
    private external fun onAdapterConnected()

    @JvmStatic
    private external fun onAdapterDisconnected()
}
