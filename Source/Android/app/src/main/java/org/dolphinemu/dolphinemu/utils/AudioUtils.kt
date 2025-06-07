package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.media.AudioManager
import androidx.annotation.Keep
import org.dolphinemu.dolphinemu.DolphinApplication

object AudioUtils {
    @JvmStatic @Keep
    fun getSampleRate(): Int =
        getAudioServiceProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE, 48000)

    @JvmStatic @Keep
    fun getFramesPerBuffer(): Int =
        getAudioServiceProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER, 256)

    private fun getAudioServiceProperty(property: String, fallback: Int): Int {
        return try {
            val context = DolphinApplication.getAppContext()
            val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            Integer.parseUnsignedInt(am.getProperty(property))
        } catch (e: NullPointerException) {
            fallback
        } catch (e: NumberFormatException) {
            fallback
        }
    }
}
