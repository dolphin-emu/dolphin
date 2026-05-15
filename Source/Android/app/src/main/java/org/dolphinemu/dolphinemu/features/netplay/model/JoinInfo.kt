// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.netplay.model

import androidx.annotation.StringRes
import org.dolphinemu.dolphinemu.R

enum class JoinInfoType(@StringRes val labelId: Int) {
    ROOM_ID(R.string.netplay_address_type_room_id),
    EXTERNAL(R.string.netplay_address_type_external),
    LOCAL(R.string.netplay_address_type_local),
}

sealed class JoinAddress {
    data object Loading : JoinAddress()
    data class Loaded(val address: String) : JoinAddress()
    data class Unknown(val retry: () -> Unit) : JoinAddress()
}
