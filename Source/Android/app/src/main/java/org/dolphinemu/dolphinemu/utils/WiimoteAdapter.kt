// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.hardware.usb.UsbConfiguration
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.services.USBPermService
import java.util.Arrays

object WiimoteAdapter {
    private const val MAX_PAYLOAD = 23
    private const val MAX_WIIMOTES = 4
    private const val TIMEOUT = 200
    private const val NINTENDO_VENDOR_ID = 0x057e
    private const val NINTENDO_WIIMOTE_PRODUCT_ID = 0x0306

    lateinit var manager: UsbManager

    private var usbConnection: UsbDeviceConnection? = null
    private val usbInterface = arrayOfNulls<UsbInterface>(MAX_WIIMOTES)
    private val usbIn = arrayOfNulls<UsbEndpoint>(MAX_WIIMOTES)

    @Keep
    @JvmField
    val wiimotePayload: Array<ByteArray> = Array(MAX_WIIMOTES) { ByteArray(MAX_PAYLOAD) }

    private fun requestPermission() {
        val devices = manager.deviceList
        for (dev in devices.values) {
            if (dev.productId == NINTENDO_WIIMOTE_PRODUCT_ID && dev.vendorId == NINTENDO_VENDOR_ID) {
                if (!manager.hasPermission(dev)) {
                    Log.warning("Requesting permission for Wii Remote adapter")

                    val context: Context = DolphinApplication.getAppContext()
                    val intent = Intent(context, USBPermService::class.java)

                    val pendingIntent =
                        PendingIntent.getService(context, 0, intent, PendingIntent.FLAG_IMMUTABLE)

                    manager.requestPermission(dev, pendingIntent)
                }
            }
        }
    }

    @Keep
    @JvmStatic
    fun queryAdapter(): Boolean {
        val devices = manager.deviceList
        for (dev in devices.values) {
            if (dev.productId == NINTENDO_WIIMOTE_PRODUCT_ID && dev.vendorId == NINTENDO_VENDOR_ID) {
                if (manager.hasPermission(dev)) {
                    return true
                } else {
                    requestPermission()
                }
            }
        }
        return false
    }

    @Keep
    @JvmStatic
    fun input(index: Int): Int {
        return usbConnection!!.bulkTransfer(
            usbIn[index], wiimotePayload[index], MAX_PAYLOAD, TIMEOUT
        )
    }

    @Keep
    @JvmStatic
    fun output(index: Int, buf: ByteArray, size: Int): Int {
        val reportNumber = buf[0]

        // Remove the report number from the buffer
        val outputBuffer = Arrays.copyOfRange(buf, 1, buf.size)
        val outputSize = size - 1

        val libusbRequestTypeClass = 1 shl 5
        val libusbRecipientInterface = 0x1
        val libusbEndpointOut = 0

        val hidSetReport = 0x9
        val hidOutput = 2 shl 8

        val write = usbConnection!!.controlTransfer(
            libusbRequestTypeClass or libusbRecipientInterface or libusbEndpointOut,
            hidSetReport,
            hidOutput or reportNumber.toInt(),
            index,
            outputBuffer,
            outputSize,
            1000
        )

        if (write < 0) {
            return 0
        }

        return write + 1
    }

    @Keep
    @JvmStatic
    fun openAdapter(): Boolean {
        // If the adapter is already open. Don't attempt to do it again
        if (usbConnection != null && usbConnection!!.fileDescriptor != -1) {
            return true
        }

        val devices = manager.deviceList
        for (dev in devices.values) {
            if (dev.productId == NINTENDO_WIIMOTE_PRODUCT_ID && dev.vendorId == NINTENDO_VENDOR_ID) {
                if (manager.hasPermission(dev)) {
                    usbConnection = manager.openDevice(dev)
                    val conf: UsbConfiguration = dev.getConfiguration(0)

                    Log.info("Number of configurations: " + dev.configurationCount)
                    Log.info("Number of Interfaces: " + dev.interfaceCount)
                    Log.info("Number of Interfaces from conf: " + conf.interfaceCount)

                    // Sometimes the interface count is returned as zero.
                    // Means the device needs to be unplugged and plugged back in again
                    if (dev.interfaceCount > 0) {
                        for (i in 0 until MAX_WIIMOTES) {
                            // One interface per Wii Remote
                            usbInterface[i] = dev.getInterface(i)
                            usbConnection!!.claimInterface(usbInterface[i], true)

                            // One endpoint per Wii Remote. Input only
                            // Output reports go through the control channel.
                            usbIn[i] = usbInterface[i]!!.getEndpoint(0)
                            Log.info("Interface " + i + " endpoint count:" + usbInterface[i]!!.endpointCount)
                        }
                        return true
                    } else {
                        // XXX: Message that the device was found, but it needs to be unplugged and plugged back in?
                        usbConnection!!.close()
                    }
                }
            }
        }
        return false
    }
}
