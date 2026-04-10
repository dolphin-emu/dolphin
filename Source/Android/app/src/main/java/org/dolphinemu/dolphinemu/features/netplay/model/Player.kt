// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

data class Player(
    val pid: Int,
    val name: String,
    val revision: String,
    val ping: Int,
    val isHost: Boolean,
    val mapping: String,
)