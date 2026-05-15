package org.dolphinemu.dolphinemu.features.gba

import androidx.annotation.Keep
import java.nio.ByteBuffer

@Keep
object GbaLibrary {

    @JvmStatic
    external fun resetGbaCore(slot: Int)

    @JvmStatic
    external fun setGbaVolume(slot: Int, volume: Int)

    @Keep
    @JvmStatic
    fun onGbaFrameWithBuffer(slot: Int, buffer: ByteBuffer) {
        GbaRenderManager.onFrame(slot, buffer)
    }

    @JvmStatic
    external fun setGCLeftOffset(offset: Int)

    @JvmStatic
    external fun getGCDrawWidth(): Int
}
