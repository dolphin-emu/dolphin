// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.net.Uri
import android.view.View
import android.widget.ImageView
import coil.load
import coil.target.ImageViewTarget
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameAdapter.GameViewHolder
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.model.GameFile
import java.io.File
import java.io.FileNotFoundException

object CoilUtils {
    fun loadGameCover(
        gameViewHolder: GameViewHolder?,
        imageView: ImageView,
        gameFile: GameFile,
        customCoverUri: Uri?
    ) {
        imageView.scaleType = ImageView.ScaleType.FIT_CENTER
        val imageTarget = ImageViewTarget(imageView)
        if (customCoverUri != null) {
            imageView.load(customCoverUri) {
                error(R.drawable.no_banner)
                target(
                    onSuccess = { success ->
                        disableInnerTitle(gameViewHolder)
                        imageTarget.drawable = success
                    },
                    onError = { error ->
                        enableInnerTitle(gameViewHolder)
                        imageTarget.drawable = error
                    }
                )
            }
        } else if (BooleanSetting.MAIN_USE_GAME_COVERS.boolean) {
            imageView.load(CoverHelper.buildGameTDBUrl(gameFile, CoverHelper.getRegion(gameFile))) {
                error(R.drawable.no_banner)
                target(
                    onSuccess = { success ->
                        disableInnerTitle(gameViewHolder)
                        imageTarget.drawable = success
                    },
                    onError = { error ->
                        enableInnerTitle(gameViewHolder)
                        imageTarget.drawable = error
                    }
                )
            }
        } else {
            imageView.load(R.drawable.no_banner)
            enableInnerTitle(gameViewHolder)
        }
    }

    private fun enableInnerTitle(gameViewHolder: GameViewHolder?) {
        if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.boolean) {
            gameViewHolder.binding.textGameTitleInner.visibility = View.VISIBLE
        }
    }

    private fun disableInnerTitle(gameViewHolder: GameViewHolder?) {
        if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.boolean) {
            gameViewHolder.binding.textGameTitleInner.visibility = View.GONE
        }
    }

    fun findCustomCover(gameFile: GameFile): Uri? {
        val customCoverPath = gameFile.customCoverPath
        var customCoverUri: Uri? = null
        var customCoverExists = false
        if (ContentHandler.isContentUri(customCoverPath)) {
            try {
                customCoverUri = ContentHandler.unmangle(customCoverPath)
                customCoverExists = true
            } catch (ignored: FileNotFoundException) {
            } catch (ignored: SecurityException) {
                // Let customCoverExists remain false
            }
        } else {
            customCoverUri = Uri.parse(customCoverPath)
            customCoverExists = File(customCoverPath).exists()
        }

        return if (customCoverExists) {
            customCoverUri
        } else {
            null
        }
    }
}
