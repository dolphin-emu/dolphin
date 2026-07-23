// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

data class ControllerMapping(
    val gamecubePorts: List<Player?>,
    val wiiRemotes: List<Player?>,
) {
    fun withGamecubePort(port: Int, player: Player?) =
        copy(gamecubePorts = gamecubePorts.replace(port, player))

    fun withWiiRemote(port: Int, player: Player?) =
        copy(wiiRemotes = wiiRemotes.replace(port, player))

    companion object {
        fun emptyControllerMapping(): ControllerMapping = ControllerMapping(
            gamecubePorts = emptyList(),
            wiiRemotes = emptyList(),
        )
    }
}

private fun <T> List<T>.replace(index: Int, value: T) =
    toMutableList().also { it[index] = value }
