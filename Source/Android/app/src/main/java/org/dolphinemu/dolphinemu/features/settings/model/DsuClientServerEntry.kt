// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

data class DsuClientServerEntry(
    val description: String,
    val address: String,
    val port: Int
) {
    fun toConfigEntry(): String = "$description:$address:$port"
}

object DsuClientServerEntries {
    private const val MAX_HOST_LENGTH = 253
    private const val MAX_PORT = 65535

    private val IPV4_REGEX =
        Regex("^(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}$")
    private val HOST_LABEL_REGEX =
        Regex("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$")

    fun parseConfigEntries(configValue: String): List<DsuClientServerEntry> {
        return configValue.split(';').mapNotNull(::parseEntry)
    }

    fun formatConfigEntries(entries: List<DsuClientServerEntry>): String {
        if (entries.isEmpty()) {
            return ""
        }
        return entries.joinToString(";") { it.toConfigEntry() } + ";"
    }

    fun parsePort(portValue: String): Int? {
        val port = portValue.toIntOrNull() ?: return null
        return if (isValidPort(port)) port else null
    }

    fun isValidDescription(description: String): Boolean {
        return !description.contains(':') && !description.contains(';')
    }

    fun isValidAddress(address: String): Boolean {
        if (address.isEmpty() || address.length > MAX_HOST_LENGTH) {
            return false
        }

        if (address.contains(':') || address.contains(';') || address.contains(' ')) {
            return false
        }

        if (IPV4_REGEX.matches(address)) {
            return true
        }

        return address.split('.')
            .all { label -> HOST_LABEL_REGEX.matches(label) }
    }

    fun isValidPort(port: Int): Boolean {
        return port in 1..MAX_PORT
    }

    private fun parseEntry(entry: String): DsuClientServerEntry? {
        if (entry.isBlank()) {
            return null
        }

        val firstColon = entry.indexOf(':')
        if (firstColon == -1) {
            return null
        }

        val secondColon = entry.indexOf(':', firstColon + 1)
        if (secondColon == -1 || entry.indexOf(':', secondColon + 1) != -1) {
            return null
        }

        val description = entry.substring(0, firstColon).trim()
        val address = entry.substring(firstColon + 1, secondColon).trim()
        val port = parsePort(entry.substring(secondColon + 1).trim()) ?: return null

        if (!isValidDescription(description) || !isValidAddress(address)) {
            return null
        }

        return DsuClientServerEntry(description, address, port)
    }
}
