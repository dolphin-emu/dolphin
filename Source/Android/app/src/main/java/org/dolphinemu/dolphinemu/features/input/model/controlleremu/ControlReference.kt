// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.model.controlleremu

import androidx.annotation.Keep

/**
 * Represents a C++ ControlReference.
 *
 * The lifetime of this class is managed by C++ code. Calling methods on it after it's destroyed
 * in C++ is undefined behavior!
 */
@Keep
class ControlReference private constructor(private val pointer: Long) {
    external fun getState(): Double

    external fun getExpression(): String

    /**
     * Sets the expression for this control reference.
     *
     * @param expr The new expression
     * @return null on success, a human-readable error on failure
     */
    external fun setExpression(expr: String): String?

    external fun isInput(): Boolean
}
