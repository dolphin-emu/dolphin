// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.graphics.drawable.Drawable
import android.net.Uri
import android.view.View
import android.widget.ImageView
import coil.ImageLoader
import coil.decode.DataSource
import coil.executeBlocking
import coil.fetch.DrawableResult
import coil.fetch.FetchResult
import coil.fetch.Fetcher
import coil.imageLoader
import coil.key.Keyer
import coil.memory.MemoryCache
import coil.request.ImageRequest
import coil.request.Options
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameAdapter.GameViewHolder
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.model.GameFile
import java.io.File
import java.io.FileNotFoundException

class GameCoverFetcher(
    private val game: GameFile,
    private val options: Options
) : Fetcher {
    override suspend fun fetch(): FetchResult {
        val customCoverUri = CoilUtils.findCustomCover(game)
        val builder = ImageRequest.Builder(DolphinApplication.getAppContext())
        var dataSource = DataSource.DISK
        val drawable: Drawable? = if (customCoverUri != null) {
            val request = builder.data(customCoverUri).error(R.drawable.no_banner).build()
            DolphinApplication.getAppContext().imageLoader.executeBlocking(request).drawable
        } else if (BooleanSetting.MAIN_USE_GAME_COVERS.boolean) {
            val request = builder.data(
                CoverHelper.buildGameTDBUrl(game, CoverHelper.getRegion(game))
            ).error(R.drawable.no_banner).build()
            dataSource = DataSource.NETWORK
            DolphinApplication.getAppContext().imageLoader.executeBlocking(request).drawable
        } else {
            null
        }

        return DrawableResult(
            // In the case where the drawable is null, intentionally throw an NPE. This tells Coil
            // to load the error drawable.
            drawable = drawable!!,
            isSampled = false,
            dataSource = dataSource
        )
    }

    class Factory : Fetcher.Factory<GameFile> {
        override fun create(data: GameFile, options: Options, imageLoader: ImageLoader): Fetcher =
            GameCoverFetcher(data, options)
    }
}

class GameCoverKeyer : Keyer<GameFile> {
    override fun key(data: GameFile, options: Options): String = data.getGameId() + data.getPath()
}

object CoilUtils {
    private val imageLoader = ImageLoader.Builder(DolphinApplication.getAppContext())
        .components {
            add(GameCoverKeyer())
            add(GameCoverFetcher.Factory())
        }
        .memoryCache {
            MemoryCache.Builder(DolphinApplication.getAppContext())
                .maxSizePercent(0.9)
                .build()
        }
        .build()

    fun loadGameCover(
        gameViewHolder: GameViewHolder?,
        imageView: ImageView,
        gameFile: GameFile
    ) {
        imageView.scaleType = ImageView.ScaleType.FIT_CENTER
        val imageRequest = ImageRequest.Builder(imageView.context)
            .data(gameFile)
            .error(R.drawable.no_banner)
            .target(imageView)
            .listener(
                onSuccess = { _, _ -> disableInnerTitle(gameViewHolder) },
                onError = { _, _ -> enableInnerTitle(gameViewHolder) }
            )
            .build()
        imageLoader.enqueue(imageRequest)
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
