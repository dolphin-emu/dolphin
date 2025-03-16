// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import androidx.annotation.Keep

/**
 * Waits for the user to press inputs, and reports which inputs were pressed.
 *
 * The caller is responsible for forwarding input events from Android to ControllerInterface
 * and then calling [update].
 */
class InputDetector {
    @Keep
    private val pointer: Long

    constructor() {
        pointer = createNew()
    }

    @Keep
    private constructor(pointer: Long) {
        this.pointer = pointer
    }

    external fun finalize()

    private external fun createNew(): Long

    /**
     * Starts a detection session.
     *
     * @param defaultDevice The device to detect inputs from.
     * @param allDevices Whether to also detect inputs from devices other than the specified one.
     */
    external fun start(defaultDevice: String, allDevices: Boolean)

    /**
     * Checks what inputs are currently pressed and updates internal state.
     *
     * During a detection session, this should be called after each call to
     * [ControllerInterface.dispatchKeyEvent] and [ControllerInterface#dispatchGenericMotionEvent].
     */
    external fun update()

    /**
     * Returns whether a detection session has finished.
     *
     * A detection session can end once the user has pressed and released an input or once a timeout
     * has been reached.
     */
    external fun isComplete(): Boolean

    /**
     * Returns the result of a detection session.
     *
     * The result of each detection session is only returned once. If this method is called more
     * than once without starting a new detection session, the second call onwards will return an
     * empty string.
     *
     * @param defaultDevice The device to detect inputs from. Should normally be the same as the one
     *                      passed to [start].
     *
     * @return The input(s) pressed by the user in the form of an InputCommon expression,
     *         or an empty string if there were no inputs.
     */
    external fun takeResults(defaultDevice: String): String
}
