package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

enum class NetworkMode(val configValue: String, @StringRes val labelId: Int) {
    FAIR_INPUT_DELAY("fixeddelay", R.string.netplay_network_mode_fair_input_delay),
    HOST_INPUT_AUTHORITY("hostinputauthority", R.string.netplay_network_mode_host_input_authority),
    GOLF("golf", R.string.netplay_network_mode_golf);

    val isHostInputAuthority: Boolean
        get() = this == HOST_INPUT_AUTHORITY || this == GOLF

    companion object {
        fun fromConfigValue(value: String): NetworkMode =
            entries.find { it.configValue == value } ?: FAIR_INPUT_DELAY
    }
}
