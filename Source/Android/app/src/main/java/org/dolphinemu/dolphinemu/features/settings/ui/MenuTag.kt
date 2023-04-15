// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController

enum class MenuTag {
    SETTINGS("settings"),
    CONFIG("config"),
    CONFIG_GENERAL("config_general"),
    CONFIG_INTERFACE("config_interface"),
    CONFIG_AUDIO("config_audio"),
    CONFIG_PATHS("config_paths"),
    CONFIG_GAME_CUBE("config_gamecube"),
    CONFIG_SERIALPORT1("config_serialport1"),
    CONFIG_WII("config_wii"),
    CONFIG_ADVANCED("config_advanced"),
    CONFIG_LOG("config_log"),
    DEBUG("debug"),
    GRAPHICS("graphics"),
    ENHANCEMENTS("enhancements"),
    STEREOSCOPY("stereoscopy"),
    HACKS("hacks"),
    STATISTICS("statistics"),
    ADVANCED_GRAPHICS("advanced_graphics"),
    GCPAD_TYPE("gc_pad_type"),
    WIIMOTE("wiimote"),
    WIIMOTE_EXTENSION("wiimote_extension"),
    GCPAD_1("gcpad", 0),
    GCPAD_2("gcpad", 1),
    GCPAD_3("gcpad", 2),
    GCPAD_4("gcpad", 3),
    WIIMOTE_1("wiimote", 0),
    WIIMOTE_2("wiimote", 1),
    WIIMOTE_3("wiimote", 2),
    WIIMOTE_4("wiimote", 3),
    WIIMOTE_EXTENSION_1("wiimote_extension", 0),
    WIIMOTE_EXTENSION_2("wiimote_extension", 1),
    WIIMOTE_EXTENSION_3("wiimote_extension", 2),
    WIIMOTE_EXTENSION_4("wiimote_extension", 3),
    WIIMOTE_GENERAL_1("wiimote_general", 0),
    WIIMOTE_GENERAL_2("wiimote_general", 1),
    WIIMOTE_GENERAL_3("wiimote_general", 2),
    WIIMOTE_GENERAL_4("wiimote_general", 3),
    WIIMOTE_MOTION_SIMULATION_1("wiimote_motion_simulation", 0),
    WIIMOTE_MOTION_SIMULATION_2("wiimote_motion_simulation", 1),
    WIIMOTE_MOTION_SIMULATION_3("wiimote_motion_simulation", 2),
    WIIMOTE_MOTION_SIMULATION_4("wiimote_motion_simulation", 3),
    WIIMOTE_MOTION_INPUT_1("wiimote_motion_input", 0),
    WIIMOTE_MOTION_INPUT_2("wiimote_motion_input", 1),
    WIIMOTE_MOTION_INPUT_3("wiimote_motion_input", 2),
    WIIMOTE_MOTION_INPUT_4("wiimote_motion_input", 3);

    var tag: String
        private set
    var subType = -1
        private set

    constructor(tag: String) {
        this.tag = tag
    }

    constructor(tag: String, subtype: Int) {
        this.tag = tag
        subType = subtype
    }

    override fun toString(): String {
        return if (subType != -1) {
            tag + subType
        } else tag
    }

    val correspondingEmulatedController: EmulatedController
        get() = if (isGCPadMenu) EmulatedController.getGcPad(subType) else if (isWiimoteMenu) EmulatedController.getWiimote(
            subType
        ) else throw UnsupportedOperationException()

    val isSerialPort1Menu: Boolean
        get() = this == CONFIG_SERIALPORT1

    val isGCPadMenu: Boolean
        get() = this == GCPAD_1 || this == GCPAD_2 || this == GCPAD_3 || this == GCPAD_4

    val isWiimoteMenu: Boolean
        get() = this == WIIMOTE_1 || this == WIIMOTE_2 || this == WIIMOTE_3 || this == WIIMOTE_4

    val isWiimoteExtensionMenu: Boolean
        get() = this == WIIMOTE_EXTENSION_1 || this == WIIMOTE_EXTENSION_2 || this == WIIMOTE_EXTENSION_3 || this == WIIMOTE_EXTENSION_4

    companion object {
        @JvmStatic
        fun getGCPadMenuTag(subtype: Int): MenuTag {
            return getMenuTag("gcpad", subtype)
        }

        @JvmStatic
        fun getWiimoteMenuTag(subtype: Int): MenuTag {
            return getMenuTag("wiimote", subtype)
        }

        @JvmStatic
        fun getWiimoteExtensionMenuTag(subtype: Int): MenuTag {
            return getMenuTag("wiimote_extension", subtype)
        }

        @JvmStatic
        fun getWiimoteGeneralMenuTag(subtype: Int): MenuTag {
            return getMenuTag("wiimote_general", subtype)
        }

        @JvmStatic
        fun getWiimoteMotionSimulationMenuTag(subtype: Int): MenuTag {
            return getMenuTag("wiimote_motion_simulation", subtype)
        }

        @JvmStatic
        fun getWiimoteMotionInputMenuTag(subtype: Int): MenuTag {
            return getMenuTag("wiimote_motion_input", subtype)
        }

        private fun getMenuTag(tag: String, subtype: Int): MenuTag {
            for (menuTag in values()) {
                if (menuTag.tag == tag && menuTag.subType == subtype) return menuTag
            }
            throw IllegalArgumentException(
                "You are asking for a menu that is not available or " +
                        "passing a wrong subtype"
            )
        }
    }
}
