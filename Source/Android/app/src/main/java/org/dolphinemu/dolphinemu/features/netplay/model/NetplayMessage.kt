package org.dolphinemu.dolphinemu.features.netplay.model

import android.content.Context
import org.dolphinemu.dolphinemu.R

sealed class NetplayMessage {
    abstract fun message(context: Context): String

    class Chat(private val chatMessage: String) : NetplayMessage() {
        override fun message(context: Context) = chatMessage
    }

    class GameChanged(private val game: String) : NetplayMessage() {
        override fun message(context: Context) =
            context.getString(R.string.netplay_message_game_changed, game)
    }

    class HostInputAuthorityChanged(private val hostInputAuthorityEnabled: Boolean) : NetplayMessage() {
        override fun message(context: Context) = context.getString(
            R.string.netplay_message_host_input_authority_changed,
            if (hostInputAuthorityEnabled) "enabled" else "disabled"
        )
    }

    class BufferChanged(private val buffer: Int) : NetplayMessage() {
        override fun message(context: Context) =
            context.getString(R.string.netplay_message_buffer_changed, buffer)
    }
}
