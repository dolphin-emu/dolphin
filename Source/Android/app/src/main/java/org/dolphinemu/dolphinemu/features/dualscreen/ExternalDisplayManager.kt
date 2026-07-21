// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.dualscreen

import android.content.Context
import android.hardware.display.DisplayManager
import android.os.Handler
import android.os.Looper
import android.view.Display
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting

class ExternalDisplayManager(private val context: Context) : DisplayManager.DisplayListener,
    GbaHostBridge.Listener {
    private val displayManager =
        context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
    private val mainHandler = Handler(Looper.getMainLooper())
    private var presentation: DualScreenPresentation? = null
    private var started = false

    fun start() {
        if (started) {
            refresh()
            return
        }

        started = true
        displayManager.registerDisplayListener(this, mainHandler)
        GbaHostBridge.addListener(this)
        refresh()
    }

    fun stop() {
        if (!started) {
            return
        }

        dismissPresentation()
        GbaHostBridge.removeListener(this)
        displayManager.unregisterDisplayListener(this)
        started = false
    }

    fun refresh() {
        if (!started || !BooleanSetting.MAIN_DUAL_SCREEN_EXTERNAL_DISPLAY.boolean) {
            dismissPresentation()
            return
        }

        val display = findPresentationDisplay(displayManager)
        if (display == null) {
            dismissPresentation()
            return
        }

        val mode = resolveMode()
        if (mode == DualScreenPresentation.Mode.EMPTY) {
            dismissPresentation()
            return
        }

        if (presentation?.display?.displayId != display.displayId) {
            dismissPresentation()
            presentation = DualScreenPresentation(context, display).also {
                it.show()
            }
        }

        presentation?.showMode(mode)
        presentation?.refreshGbaSettings()
    }

    fun refreshPointerSettings() {
        presentation?.refreshPointerSettings()
    }

    fun hasPresentationDisplay(): Boolean = findPresentationDisplay(displayManager) != null

    override fun onDisplayAdded(displayId: Int) = refresh()

    override fun onDisplayRemoved(displayId: Int) = refresh()

    override fun onDisplayChanged(displayId: Int) = refresh()

    override fun onGbaGameChanged(info: GbaHostBridge.CoreInfo) = refresh()

    override fun onGbaVisibleDeviceChanged(deviceNumber: Int, info: GbaHostBridge.CoreInfo?) =
        refresh()

    private fun dismissPresentation() {
        presentation?.clearPointerState()
        presentation?.dismiss()
        presentation = null
    }

    private fun resolveMode(): DualScreenPresentation.Mode {
        if (!NativeLibrary.IsGameMetadataValid()) {
            return DualScreenPresentation.Mode.EMPTY
        }

        if (NativeLibrary.IsEmulatingWii()) {
            return if (
                BooleanSetting.MAIN_DUAL_SCREEN_WII_POINTER_TOUCHPAD.boolean &&
                NativeLibrary.IsRunning() &&
                NativeLibrary.HasSurface()
            ) {
                DualScreenPresentation.Mode.WII_POINTER
            } else {
                DualScreenPresentation.Mode.EMPTY
            }
        }

        return if (BooleanSetting.MAIN_GBA_LINK_EXTERNAL_DISPLAY.boolean &&
            GbaLinkSettings.hasAnyEmulatedGbaPortConfigured() &&
            GbaHostBridge.hasActiveCore()
        ) {
            DualScreenPresentation.Mode.GBA_SCREEN
        } else {
            DualScreenPresentation.Mode.EMPTY
        }
    }

    companion object {
        fun hasPresentationDisplay(context: Context): Boolean {
            val displayManager =
                context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
            return findPresentationDisplay(displayManager) != null
        }

        private fun findPresentationDisplay(displayManager: DisplayManager): Display? =
            displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION).firstOrNull {
                it.displayId != Display.DEFAULT_DISPLAY && it.state == Display.STATE_ON
            }
    }
}
