// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

object NativeConfig {
    const val LAYER_BASE_OR_CURRENT = 0
    const val LAYER_BASE = 1
    const val LAYER_LOCAL_GAME = 2
    const val LAYER_ACTIVE = 3
    const val LAYER_CURRENT = 4

    @JvmStatic
    external fun isSettingSaveable(file: String, section: String, key: String): Boolean

    @JvmStatic
    external fun loadGameInis(gameId: String, revision: Int)

    @JvmStatic
    external fun unloadGameInis()

    @JvmStatic
    external fun save(layer: Int)

    @JvmStatic
    external fun deleteAllKeys(layer: Int)

    @JvmStatic
    external fun isOverridden(file: String, section: String, key: String): Boolean

    @JvmStatic
    external fun deleteKey(layer: Int, file: String, section: String, key: String): Boolean

    @JvmStatic
    external fun exists(layer: Int, file: String, section: String, key: String): Boolean

    @JvmStatic
    external fun getString(
        layer: Int,
        file: String,
        section: String,
        key: String,
        defaultValue: String
    ): String

    @JvmStatic
    external fun getBoolean(
        layer: Int,
        file: String,
        section: String,
        key: String,
        defaultValue: Boolean
    ): Boolean

    @JvmStatic
    external fun getInt(
        layer: Int,
        file: String,
        section: String,
        key: String,
        defaultValue: Int
    ): Int

    @JvmStatic
    external fun getFloat(
        layer: Int,
        file: String,
        section: String,
        key: String,
        defaultValue: Float
    ): Float

    @JvmStatic
    external fun setString(
        layer: Int,
        file: String,
        section: String,
        key: String,
        value: String
    )

    @JvmStatic
    external fun setBoolean(
        layer: Int,
        file: String,
        section: String,
        key: String,
        value: Boolean
    )

    @JvmStatic
    external fun setInt(
        layer: Int,
        file: String,
        section: String,
        key: String,
        value: Int
    )

    @JvmStatic
    external fun setFloat(
        layer: Int,
        file: String,
        section: String,
        key: String,
        value: Float
    )
}
