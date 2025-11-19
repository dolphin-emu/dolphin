/*
 * Copyright 2013 Dolphin Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

package org.dolphinemu.dolphinemu

import android.content.res.Resources
import android.view.Surface
import android.widget.Toast
import androidx.annotation.Keep
import androidx.core.content.ContextCompat
import androidx.core.util.Pair
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.dialogs.AlertMessage
import org.dolphinemu.dolphinemu.utils.CompressCallback
import org.dolphinemu.dolphinemu.utils.Log
import java.lang.ref.WeakReference
import java.util.concurrent.Semaphore

/**
 * Class which contains methods that interact
 * with the native side of the Dolphin code.
 */
object NativeLibrary {
    /**
     * Button type, for legacy use only.
     */
    object ButtonType {
        const val BUTTON_A = 0
        const val BUTTON_B = 1
        const val BUTTON_START = 2
        const val BUTTON_X = 3
        const val BUTTON_Y = 4
        const val BUTTON_Z = 5
        const val BUTTON_UP = 6
        const val BUTTON_DOWN = 7
        const val BUTTON_LEFT = 8
        const val BUTTON_RIGHT = 9
        const val STICK_MAIN = 10
        const val STICK_MAIN_UP = 11
        const val STICK_MAIN_DOWN = 12
        const val STICK_MAIN_LEFT = 13
        const val STICK_MAIN_RIGHT = 14
        const val STICK_C = 15
        const val STICK_C_UP = 16
        const val STICK_C_DOWN = 17
        const val STICK_C_LEFT = 18
        const val STICK_C_RIGHT = 19
        const val TRIGGER_L = 20
        const val TRIGGER_R = 21
        const val WIIMOTE_BUTTON_A = 100
        const val WIIMOTE_BUTTON_B = 101
        const val WIIMOTE_BUTTON_MINUS = 102
        const val WIIMOTE_BUTTON_PLUS = 103
        const val WIIMOTE_BUTTON_HOME = 104
        const val WIIMOTE_BUTTON_1 = 105
        const val WIIMOTE_BUTTON_2 = 106
        const val WIIMOTE_UP = 107
        const val WIIMOTE_DOWN = 108
        const val WIIMOTE_LEFT = 109
        const val WIIMOTE_RIGHT = 110
        const val WIIMOTE_IR = 111
        const val WIIMOTE_IR_UP = 112
        const val WIIMOTE_IR_DOWN = 113
        const val WIIMOTE_IR_LEFT = 114
        const val WIIMOTE_IR_RIGHT = 115
        const val WIIMOTE_IR_FORWARD = 116
        const val WIIMOTE_IR_BACKWARD = 117
        const val WIIMOTE_IR_HIDE = 118
        const val WIIMOTE_SWING = 119
        const val WIIMOTE_SWING_UP = 120
        const val WIIMOTE_SWING_DOWN = 121
        const val WIIMOTE_SWING_LEFT = 122
        const val WIIMOTE_SWING_RIGHT = 123
        const val WIIMOTE_SWING_FORWARD = 124
        const val WIIMOTE_SWING_BACKWARD = 125
        const val WIIMOTE_TILT = 126
        const val WIIMOTE_TILT_FORWARD = 127
        const val WIIMOTE_TILT_BACKWARD = 128
        const val WIIMOTE_TILT_LEFT = 129
        const val WIIMOTE_TILT_RIGHT = 130
        const val WIIMOTE_TILT_MODIFIER = 131
        const val WIIMOTE_SHAKE_X = 132
        const val WIIMOTE_SHAKE_Y = 133
        const val WIIMOTE_SHAKE_Z = 134
        const val NUNCHUK_BUTTON_C = 200
        const val NUNCHUK_BUTTON_Z = 201
        const val NUNCHUK_STICK = 202
        const val NUNCHUK_STICK_UP = 203
        const val NUNCHUK_STICK_DOWN = 204
        const val NUNCHUK_STICK_LEFT = 205
        const val NUNCHUK_STICK_RIGHT = 206
        const val NUNCHUK_SWING = 207
        const val NUNCHUK_SWING_UP = 208
        const val NUNCHUK_SWING_DOWN = 209
        const val NUNCHUK_SWING_LEFT = 210
        const val NUNCHUK_SWING_RIGHT = 211
        const val NUNCHUK_SWING_FORWARD = 212
        const val NUNCHUK_SWING_BACKWARD = 213
        const val NUNCHUK_TILT = 214
        const val NUNCHUK_TILT_FORWARD = 215
        const val NUNCHUK_TILT_BACKWARD = 216
        const val NUNCHUK_TILT_LEFT = 217
        const val NUNCHUK_TILT_RIGHT = 218
        const val NUNCHUK_TILT_MODIFIER = 219
        const val NUNCHUK_SHAKE_X = 220
        const val NUNCHUK_SHAKE_Y = 221
        const val NUNCHUK_SHAKE_Z = 222
        const val CLASSIC_BUTTON_A = 300
        const val CLASSIC_BUTTON_B = 301
        const val CLASSIC_BUTTON_X = 302
        const val CLASSIC_BUTTON_Y = 303
        const val CLASSIC_BUTTON_MINUS = 304
        const val CLASSIC_BUTTON_PLUS = 305
        const val CLASSIC_BUTTON_HOME = 306
        const val CLASSIC_BUTTON_ZL = 307
        const val CLASSIC_BUTTON_ZR = 308
        const val CLASSIC_DPAD_UP = 309
        const val CLASSIC_DPAD_DOWN = 310
        const val CLASSIC_DPAD_LEFT = 311
        const val CLASSIC_DPAD_RIGHT = 312
        const val CLASSIC_STICK_LEFT = 313
        const val CLASSIC_STICK_LEFT_UP = 314
        const val CLASSIC_STICK_LEFT_DOWN = 315
        const val CLASSIC_STICK_LEFT_LEFT = 316
        const val CLASSIC_STICK_LEFT_RIGHT = 317
        const val CLASSIC_STICK_RIGHT = 318
        const val CLASSIC_STICK_RIGHT_UP = 319
        const val CLASSIC_STICK_RIGHT_DOWN = 320
        const val CLASSIC_STICK_RIGHT_LEFT = 321
        const val CLASSIC_STICK_RIGHT_RIGHT = 322
        const val CLASSIC_TRIGGER_L = 323
        const val CLASSIC_TRIGGER_R = 324
        const val GUITAR_BUTTON_MINUS = 400
        const val GUITAR_BUTTON_PLUS = 401
        const val GUITAR_FRET_GREEN = 402
        const val GUITAR_FRET_RED = 403
        const val GUITAR_FRET_YELLOW = 404
        const val GUITAR_FRET_BLUE = 405
        const val GUITAR_FRET_ORANGE = 406
        const val GUITAR_STRUM_UP = 407
        const val GUITAR_STRUM_DOWN = 408
        const val GUITAR_STICK = 409
        const val GUITAR_STICK_UP = 410
        const val GUITAR_STICK_DOWN = 411
        const val GUITAR_STICK_LEFT = 412
        const val GUITAR_STICK_RIGHT = 413
        const val GUITAR_WHAMMY_BAR = 414
        const val DRUMS_BUTTON_MINUS = 500
        const val DRUMS_BUTTON_PLUS = 501
        const val DRUMS_PAD_RED = 502
        const val DRUMS_PAD_YELLOW = 503
        const val DRUMS_PAD_BLUE = 504
        const val DRUMS_PAD_GREEN = 505
        const val DRUMS_PAD_ORANGE = 506
        const val DRUMS_PAD_BASS = 507
        const val DRUMS_STICK = 508
        const val DRUMS_STICK_UP = 509
        const val DRUMS_STICK_DOWN = 510
        const val DRUMS_STICK_LEFT = 511
        const val DRUMS_STICK_RIGHT = 512
        const val TURNTABLE_BUTTON_GREEN_LEFT = 600
        const val TURNTABLE_BUTTON_RED_LEFT = 601
        const val TURNTABLE_BUTTON_BLUE_LEFT = 602
        const val TURNTABLE_BUTTON_GREEN_RIGHT = 603
        const val TURNTABLE_BUTTON_RED_RIGHT = 604
        const val TURNTABLE_BUTTON_BLUE_RIGHT = 605
        const val TURNTABLE_BUTTON_MINUS = 606
        const val TURNTABLE_BUTTON_PLUS = 607
        const val TURNTABLE_BUTTON_HOME = 608
        const val TURNTABLE_BUTTON_EUPHORIA = 609
        const val TURNTABLE_TABLE_LEFT = 610
        const val TURNTABLE_TABLE_LEFT_LEFT = 611
        const val TURNTABLE_TABLE_LEFT_RIGHT = 612
        const val TURNTABLE_TABLE_RIGHT = 613
        const val TURNTABLE_TABLE_RIGHT_LEFT = 614
        const val TURNTABLE_TABLE_RIGHT_RIGHT = 615
        const val TURNTABLE_STICK = 616
        const val TURNTABLE_STICK_UP = 617
        const val TURNTABLE_STICK_DOWN = 618
        const val TURNTABLE_STICK_LEFT = 619
        const val TURNTABLE_STICK_RIGHT = 620
        const val TURNTABLE_EFFECT_DIAL = 621
        const val TURNTABLE_CROSSFADE = 622
        const val TURNTABLE_CROSSFADE_LEFT = 623
        const val TURNTABLE_CROSSFADE_RIGHT = 624
        const val WIIMOTE_ACCEL_LEFT = 625
        const val WIIMOTE_ACCEL_RIGHT = 626
        const val WIIMOTE_ACCEL_FORWARD = 627
        const val WIIMOTE_ACCEL_BACKWARD = 628
        const val WIIMOTE_ACCEL_UP = 629
        const val WIIMOTE_ACCEL_DOWN = 630
        const val WIIMOTE_GYRO_PITCH_UP = 631
        const val WIIMOTE_GYRO_PITCH_DOWN = 632
        const val WIIMOTE_GYRO_ROLL_LEFT = 633
        const val WIIMOTE_GYRO_ROLL_RIGHT = 634
        const val WIIMOTE_GYRO_YAW_LEFT = 635
        const val WIIMOTE_GYRO_YAW_RIGHT = 636
    }

    object ButtonState {
        const val RELEASED = 0
        const val PRESSED = 1
    }

    private val alertMessageSemaphore = Semaphore(0)

    @Volatile
    private var isShowingAlertMessage = false
    private var emulationActivityRef = WeakReference<EmulationActivity?>(null)

    /**
     * Returns the current instance of EmulationActivity.
     * There should only ever be one EmulationActivity instantiated.
     */
    @JvmStatic
    fun getEmulationActivity(): EmulationActivity? = emulationActivityRef.get()

    @JvmStatic
    external fun GetVersionString(): String

    @JvmStatic
    external fun GetGitRevision(): String

    /**
     * Saves a screen capture of the game.
     */
    @JvmStatic
    external fun SaveScreenShot()

    /**
     * Saves a game state to the slot number.
     *
     * @param slot The slot location to save state to.
     * @param wait If false, returns as early as possible. If true, returns once the savestate has been written to disk.
     */
    @JvmStatic
    external fun SaveState(slot: Int, wait: Boolean)

    /**
     * Saves a game state to the specified path.
     *
     * @param path The path to save state to.
     * @param wait If false, returns as early as possible. If true, returns once the savestate has been written to disk.
     */
    @JvmStatic
    external fun SaveStateAs(path: String, wait: Boolean)

    /**
     * Loads a game state from the slot number.
     *
     * @param slot The slot location to load state from.
     */
    @JvmStatic
    external fun LoadState(slot: Int)

    /**
     * Loads a game state from the specified path.
     *
     * @param path The path to load state from.
     */
    @JvmStatic
    external fun LoadStateAs(path: String)

    /**
     * Returns when the savestate in the given slot was created, or 0 if the slot is empty.
     */
    @JvmStatic
    external fun GetUnixTimeOfStateSlot(slot: Int): Long

    /**
     * Sets the current working user directory. If not set, it auto-detects a location.
     */
    @JvmStatic
    external fun SetUserDirectory(directory: String)

    /**
     * Returns the current working user directory.
     */
    @JvmStatic
    external fun GetUserDirectory(): String

    @JvmStatic
    external fun SetCacheDirectory(directory: String)

    @JvmStatic
    external fun GetCacheDirectory(): String

    @JvmStatic
    external fun DefaultCPUCore(): Int

    @JvmStatic
    external fun GetDefaultGraphicsBackendConfigName(): String

    @JvmStatic
    external fun GetMaxLogLevel(): Int

    @JvmStatic
    external fun ReloadConfig()

    @JvmStatic
    external fun ResetDolphinSettings()

    @JvmStatic
    external fun UpdateGCAdapterScanThread()

    /**
     * Initializes the native parts of the app.
     *
     * Should be called at app start before running any other native code
     * (other than the native methods in DirectoryInitialization).
     */
    @JvmStatic
    external fun Initialize()

    /**
     * Tells analytics that Dolphin has been started.
     *
     * Since users typically don't explicitly close Android apps, it's appropriate to
     * call this not only when the app starts but also when the user returns to the app
     * after not using it for a significant amount of time.
     */
    @JvmStatic
    external fun ReportStartToAnalytics()

    @JvmStatic
    external fun GenerateNewStatisticsId()

    /**
     * Begins emulation.
     */
    @JvmStatic
    external fun Run(path: Array<String>, riivolution: Boolean)

    /**
     * Begins emulation from the specified savestate.
     */
    @JvmStatic
    external fun Run(
        path: Array<String>, riivolution: Boolean, savestatePath: String, deleteSavestate: Boolean
    )

    /**
     * Begins emulation of the System Menu.
     */
    @JvmStatic
    external fun RunSystemMenu()

    @JvmStatic
    external fun ChangeDisc(path: String)

    @JvmStatic
    external fun SurfaceChanged(surf: Surface)

    @JvmStatic
    external fun SurfaceDestroyed()

    @JvmStatic
    external fun HasSurface(): Boolean

    @JvmStatic
    external fun UnPauseEmulation()

    @JvmStatic
    external fun PauseEmulation(overrideAchievementRestrictions: Boolean)

    @JvmStatic
    external fun StopEmulation()

  /**
   * Ensures that IsRunning will return true from now on until emulation exits.
   * (If this is not called, IsRunning will start returning true at some point
   * after calling Run.)
   */
    @JvmStatic
    external fun SetIsBooting()

    /**
     * Returns true if emulation is running or paused.
     */
    @JvmStatic
    external fun IsRunning(): Boolean

    /**
     * Returns true if emulation is running and not paused.
     */
    @JvmStatic
    external fun IsRunningAndUnpaused(): Boolean

    /**
     * Returns true if emulation is fully shut down.
     */
    @JvmStatic
    external fun IsUninitialized(): Boolean

    /**
     * Re-initializes software JitBlock profiling data.
     */
    @JvmStatic
    external fun WipeJitBlockProfilingData()

    /**
     * Writes out the JitBlock Cache log dump.
     */
    @JvmStatic
    external fun WriteJitBlockLogDump()

    /**
     * Native EGL functions not exposed by Java bindings.
     */
    @JvmStatic
    external fun eglBindAPI(api: Int)

    /**
     * Provides a way to refresh Wiimote connections.
     */
    @JvmStatic
    external fun RefreshWiimotes()

    @JvmStatic
    external fun GetLogTypeNames(): Array<Pair<String, String>>

    @JvmStatic
    external fun ReloadLoggerConfig()

    @JvmStatic
    external fun ConvertDiscImage(
        inPath: String,
        outPath: String,
        platform: Int,
        format: Int,
        blockSize: Int,
        compression: Int,
        compressionLevel: Int,
        scrub: Boolean,
        callback: CompressCallback
    ): Boolean

    @JvmStatic
    external fun FormatSize(bytes: Long, decimals: Int): String

    @JvmStatic
    external fun SetObscuredPixelsLeft(width: Int)

    @JvmStatic
    external fun SetObscuredPixelsTop(height: Int)

    @JvmStatic
    external fun IsGameMetadataValid(): Boolean

    @JvmStatic
    external fun GetGameAspectRatio(): Float

    @JvmStatic
    fun IsEmulatingWii(): Boolean {
        checkGameMetadataValid()
        return IsEmulatingWiiUnchecked()
    }

    @JvmStatic
    fun GetCurrentGameID(): String {
        checkGameMetadataValid()
        return GetCurrentGameIDUnchecked()
    }

    @JvmStatic
    fun GetCurrentTitleDescription(): String {
        checkGameMetadataValid()
        return GetCurrentTitleDescriptionUnchecked()
    }

    @JvmStatic
    @Keep
    fun displayToastMsg(text: String, long_length: Boolean) {
        val context = DolphinApplication.getAppContext()
        val length = if (long_length) Toast.LENGTH_LONG else Toast.LENGTH_SHORT
        ContextCompat.getMainExecutor(context).execute {
            Toast.makeText(context, text, length).show()
        }
    }

    @JvmStatic
    @Keep
    fun displayAlertMsg(
        caption: String, text: String, yesNo: Boolean, isWarning: Boolean, nonBlocking: Boolean
    ): Boolean {
        Log.error("[NativeLibrary] Alert: $text")
        val emulationActivity = emulationActivityRef.get()
        var result = false

        // We can't use AlertMessages unless we have a non-null activity reference
        // and are allowed to block. As a fallback, we can use toasts.
        if (emulationActivity == null || nonBlocking) {
            displayToastMsg(text, true)
        } else {
            isShowingAlertMessage = true

            emulationActivity.runOnUiThread {
                val fragmentManager = emulationActivity.supportFragmentManager
                if (fragmentManager.isStateSaved) {
                    // The activity is being destroyed, so we can't use it to display an AlertMessage.
                    // Fall back to a toast.
                    Toast.makeText(emulationActivity, text, Toast.LENGTH_LONG).show()
                    NotifyAlertMessageLock()
                } else {
                    AlertMessage.newInstance(caption, text, yesNo, isWarning)
                        .show(fragmentManager, "AlertMessage")
                }
            }

            // Wait for the lock to notify that it is complete.
            try {
                alertMessageSemaphore.acquire()
            } catch (ignored: InterruptedException) {
                Thread.currentThread().interrupt()
            }

            if (yesNo) {
                result = AlertMessage.getAlertResult()
            }
        }

        isShowingAlertMessage = false
        return result
    }

    @JvmStatic
    fun IsShowingAlertMessage(): Boolean = isShowingAlertMessage

    @JvmStatic
    fun NotifyAlertMessageLock() {
        alertMessageSemaphore.release()
    }

    @JvmStatic
    fun setEmulationActivity(emulationActivity: EmulationActivity) {
        Log.verbose("[NativeLibrary] Registering EmulationActivity.")
        emulationActivityRef = WeakReference(emulationActivity)
    }

    @JvmStatic
    fun clearEmulationActivity() {
        Log.verbose("[NativeLibrary] Unregistering EmulationActivity.")
        emulationActivityRef.clear()
    }

    @JvmStatic
    @Keep
    fun finishEmulationActivity() {
        val emulationActivity = emulationActivityRef.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null.")
        } else {
            Log.verbose("[NativeLibrary] Finishing EmulationActivity.")
            emulationActivity.runOnUiThread(emulationActivity::finish)
        }
    }

    @JvmStatic
    @Keep
    fun updateTouchPointer() {
        val emulationActivity = emulationActivityRef.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null.")
        } else {
            emulationActivity.runOnUiThread(emulationActivity::initInputPointer)
        }
    }

    @JvmStatic
    @Keep
    fun onTitleChanged() {
        val emulationActivity = emulationActivityRef.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null.")
        } else {
            emulationActivity.runOnUiThread(emulationActivity::onTitleChanged)
        }
    }

    @JvmStatic
    @Keep
    fun getRenderSurfaceScale(): Float {
        val emulationActivity = emulationActivityRef.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null.")
            return Resources.getSystem().displayMetrics.scaledDensity
        }

        return emulationActivity.resources.displayMetrics.scaledDensity
    }

    private fun checkGameMetadataValid() {
        check(IsGameMetadataValid()) { "No game is running" }
    }

    private external fun IsEmulatingWiiUnchecked(): Boolean

    private external fun GetCurrentGameIDUnchecked(): String

    private external fun GetCurrentTitleDescriptionUnchecked(): String
}
