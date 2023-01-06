// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import org.dolphinemu.dolphinemu.utils.CoverHelper.buildGameTDBUrl
import org.dolphinemu.dolphinemu.utils.CoverHelper.getRegion
import org.dolphinemu.dolphinemu.utils.CoverHelper.saveCover
import android.os.Looper
import org.dolphinemu.dolphinemu.model.GameFile
import com.bumptech.glide.Glide
import com.bumptech.glide.load.engine.DiskCacheStrategy
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameAdapter.GameViewHolder
import android.app.Activity
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import com.bumptech.glide.request.RequestListener
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Handler
import android.view.View
import android.widget.ImageView
import com.bumptech.glide.load.DataSource
import com.bumptech.glide.load.engine.GlideException
import com.bumptech.glide.request.target.CustomTarget
import com.bumptech.glide.request.target.Target
import com.bumptech.glide.request.transition.Transition
import java.io.File
import java.io.FileNotFoundException
import java.util.concurrent.Executors

object GlideUtils {
    private val saveCoverExecutor = Executors.newSingleThreadExecutor()
    private val unmangleExecutor = Executors.newSingleThreadExecutor()
    private val unmangleHandler = Handler(Looper.getMainLooper())

    fun loadGameBanner(imageView: ImageView, gameFile: GameFile) {
        val context = imageView.context
        val vector = gameFile.banner
        val width = gameFile.bannerWidth
        val height = gameFile.bannerHeight
        if (width > 0 && height > 0) {
            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            bitmap.setPixels(vector, 0, width, 0, 0, width, height)
            Glide.with(context)
                .load(bitmap)
                .diskCacheStrategy(DiskCacheStrategy.NONE)
                .centerCrop()
                .into(imageView)
        } else {
            Glide.with(context)
                .load(R.drawable.no_banner)
                .into(imageView)
        }
    }

    fun loadGameCover(
        gameViewHolder: GameViewHolder?,
        imageView: ImageView,
        gameFile: GameFile,
        activity: Activity?
    ) {
        gameViewHolder?.apply {
            if (BooleanSetting.MAIN_SHOW_GAME_TITLES.booleanGlobal) {
                binding.textGameTitle.text = gameFile.title
                binding.textGameTitle.visibility = View.VISIBLE
                binding.textGameTitleInner.visibility = View.GONE
                binding.textGameCaption.visibility = View.VISIBLE
            } else {
                binding.textGameTitleInner.text = gameFile.title
                binding.textGameTitle.visibility = View.GONE
                binding.textGameCaption.visibility = View.GONE
            }
        }

        unmangleExecutor.execute {
            val customCoverPath = gameFile.customCoverPath
            var customCoverUri: Uri? = null
            var customCoverExists = false
            if (ContentHandler.isContentUri(customCoverPath)) {
                try {
                    customCoverUri = ContentHandler.unmangle(customCoverPath)
                    customCoverExists = true
                } catch (ignored: FileNotFoundException) {
                    // Let customCoverExists remain false
                } catch (ignored: SecurityException) {
                }
            } else {
                customCoverUri = Uri.parse(customCoverPath)
                customCoverExists = File(customCoverPath).exists()
            }

            val context = imageView.context
            val finalCustomCoverExists = customCoverExists
            val finalCustomCoverUri = customCoverUri

            val cover = File(gameFile.getCoverPath(context))
            val cachedCoverExists = cover.exists()
            unmangleHandler.post {
                // We can't get a reference to the current activity in the TV version.
                // Luckily it won't attempt to start loads on destroyed activities.
                if (activity != null) {
                    // We can't start an image load on a destroyed activity
                    if (activity.isDestroyed) {
                        return@post
                    }
                }

                if (finalCustomCoverExists) {
                    Glide.with(imageView)
                        .load(finalCustomCoverUri)
                        .diskCacheStrategy(DiskCacheStrategy.NONE)
                        .centerCrop()
                        .error(R.drawable.no_banner)
                        .listener(object : RequestListener<Drawable?> {
                            override fun onLoadFailed(
                                e: GlideException?,
                                model: Any,
                                target: Target<Drawable?>,
                                isFirstResource: Boolean
                            ): Boolean {
                                enableInnerTitle(gameViewHolder)
                                return false
                            }

                            override fun onResourceReady(
                                resource: Drawable?,
                                model: Any,
                                target: Target<Drawable?>,
                                dataSource: DataSource,
                                isFirstResource: Boolean
                            ): Boolean {
                                disableInnerTitle(gameViewHolder)
                                return false
                            }
                        })
                        .into(imageView)
                } else if (cachedCoverExists) {
                    Glide.with(imageView)
                        .load(cover)
                        .diskCacheStrategy(DiskCacheStrategy.NONE)
                        .centerCrop()
                        .error(R.drawable.no_banner)
                        .listener(object : RequestListener<Drawable?> {
                            override fun onLoadFailed(
                                e: GlideException?,
                                model: Any,
                                target: Target<Drawable?>,
                                isFirstResource: Boolean
                            ): Boolean {
                                enableInnerTitle(gameViewHolder)
                                return false
                            }

                            override fun onResourceReady(
                                resource: Drawable?,
                                model: Any,
                                target: Target<Drawable?>,
                                dataSource: DataSource,
                                isFirstResource: Boolean
                            ): Boolean {
                                disableInnerTitle(gameViewHolder)
                                return false
                            }
                        })
                        .into(imageView)
                } else if (BooleanSetting.MAIN_USE_GAME_COVERS.booleanGlobal) {
                    Glide.with(context)
                        .load(buildGameTDBUrl(gameFile, getRegion(gameFile)))
                        .diskCacheStrategy(DiskCacheStrategy.NONE)
                        .centerCrop()
                        .error(R.drawable.no_banner)
                        .listener(object : RequestListener<Drawable?> {
                            override fun onLoadFailed(
                                e: GlideException?,
                                model: Any,
                                target: Target<Drawable?>,
                                isFirstResource: Boolean
                            ): Boolean {
                                enableInnerTitle(gameViewHolder)
                                return false
                            }

                            override fun onResourceReady(
                                resource: Drawable?,
                                model: Any,
                                target: Target<Drawable?>,
                                dataSource: DataSource,
                                isFirstResource: Boolean
                            ): Boolean {
                                disableInnerTitle(gameViewHolder)
                                return false
                            }
                        })
                        .into(object : CustomTarget<Drawable?>() {
                            override fun onLoadCleared(placeholder: Drawable?) {}
                            override fun onResourceReady(
                                resource: Drawable,
                                transition: Transition<in Drawable?>?
                            ) {
                                val savedCover = (resource as BitmapDrawable).bitmap
                                saveCoverExecutor.execute {
                                    saveCover(
                                        savedCover,
                                        gameFile.getCoverPath(context)
                                    )
                                }
                                imageView.setImageBitmap(savedCover)
                            }
                        })
                } else {
                    Glide.with(imageView.context)
                        .load(R.drawable.no_banner)
                        .into(imageView)
                    enableInnerTitle(gameViewHolder)
                }
            }
        }
    }

    private fun enableInnerTitle(gameViewHolder: GameViewHolder?) {
        if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.booleanGlobal) {
            gameViewHolder.binding.textGameTitleInner.visibility = View.VISIBLE
        }
    }

    private fun disableInnerTitle(gameViewHolder: GameViewHolder?) {
        if (gameViewHolder != null && !BooleanSetting.MAIN_SHOW_GAME_TITLES.booleanGlobal) {
            gameViewHolder.binding.textGameTitleInner.visibility = View.GONE
        }
    }
}
