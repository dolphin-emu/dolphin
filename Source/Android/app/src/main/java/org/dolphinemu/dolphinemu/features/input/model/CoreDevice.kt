// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model

import androidx.annotation.Keep

/**
 * Represents a C++ ciface::Core::Device.
 */
@Keep
class CoreDevice private constructor(private val pointer: Long) {
    /**
     * Represents a C++ ciface::Core::Device::Control.
     *
     * This class is marked inner to ensure that the CoreDevice parent does not get garbage collected
     * while a Control is still accessible. (CoreDevice's finalizer may delete the native controls.)
     */
    @Keep
    inner class Control private constructor(private val pointer: Long) {
        external fun getName(): String
    }

    protected external fun finalize()

    external fun getInputs(): Array<Control>

    external fun getOutputs(): Array<Control>
}
