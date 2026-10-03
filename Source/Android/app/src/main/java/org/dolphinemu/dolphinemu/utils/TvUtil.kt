// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.app.job.JobInfo
import android.app.job.JobScheduler
import android.content.ComponentName
import android.content.ContentResolver
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.VectorDrawable
import android.net.Uri
import android.os.Build
import android.os.PersistableBundle
import android.util.Log
import androidx.annotation.AnyRes
import androidx.annotation.RequiresApi
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.FileProvider.getUriForFile
import androidx.core.graphics.createBitmap
import androidx.core.net.toUri
import androidx.tvprovider.media.tv.Channel
import androidx.tvprovider.media.tv.TvContractCompat
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.model.HomeScreenChannel
import org.dolphinemu.dolphinemu.services.SyncChannelJobService
import org.dolphinemu.dolphinemu.services.SyncProgramsJobService
import org.dolphinemu.dolphinemu.ui.platform.PlatformTab
import java.io.File
import java.io.FileNotFoundException

/**
 * Assists in TV related services, e.g., home screen channels
 */
object TvUtil {
    private const val TAG = "TvUtil"
    private const val CHANNEL_JOB_ID_OFFSET = 1000L
    private const val LEANBACK_PACKAGE = "com.google.android.tvlauncher"

    private val channelsProjection = arrayOf(
        TvContractCompat.Channels._ID,
        TvContractCompat.Channels.COLUMN_DISPLAY_NAME,
        TvContractCompat.Channels.COLUMN_BROWSABLE,
        TvContractCompat.Channels.COLUMN_APP_LINK_INTENT_URI
    )

    @JvmStatic
    fun getAllChannels(context: Context): List<Channel> {
        val channels = ArrayList<Channel>()
        val cursor = context.contentResolver.query(
            TvContractCompat.Channels.CONTENT_URI, channelsProjection, null, null, null
        )
        if (cursor != null && cursor.moveToFirst()) {
            do {
                channels.add(Channel.fromCursor(cursor))
            } while (cursor.moveToNext())
        }
        return channels
    }

    @JvmStatic
    fun getChannelById(context: Context, channelId: Long): Channel? {
        for (channel in getAllChannels(context)) {
            if (channel.id == channelId) {
                return channel
            }
        }
        return null
    }

    /**
     * Updates all Leanback homescreen channels
     */
    @JvmStatic
    fun updateAllChannels(context: Context) {
        if (Build.VERSION.SDK_INT < 26) {
            return
        }

        for (channel in getAllChannels(context)) {
            context.contentResolver.update(
                TvContractCompat.buildChannelUri(channel.id), channel.toContentValues(), null, null
            )
        }
    }

    @JvmStatic
    fun getUriToResource(context: Context, @AnyRes resId: Int): Uri {
        val res = context.resources
        return (ContentResolver.SCHEME_ANDROID_RESOURCE + "://" + res.getResourcePackageName(resId) + '/' + res.getResourceTypeName(
            resId
        ) + '/' + res.getResourceEntryName(resId)).toUri()
    }

    /**
     * Converts a resource into a [Bitmap]. If the resource is a vector drawable, it will be
     * drawn into a new Bitmap. Otherwise, the [BitmapFactory] will decode the resource.
     */
    @JvmStatic
    fun convertToBitmap(context: Context, resourceId: Int): Bitmap {
        val drawable = AppCompatResources.getDrawable(context, resourceId)
        if (drawable is VectorDrawable) {
            val bitmap = createBitmap(drawable.intrinsicWidth, drawable.intrinsicHeight)
            val canvas = Canvas(bitmap)
            drawable.setBounds(0, 0, canvas.width, canvas.height)
            drawable.draw(canvas)
            return bitmap
        }
        return BitmapFactory.decodeResource(context.resources, resourceId)
    }

    /**
     * Leanback launcher requires a uri for poster art so we create a contentUri and
     * pass that to LEANBACK_PACKAGE
     */
    @JvmStatic
    fun buildBanner(game: GameFile, context: Context): Uri? {
        var contentUri: Uri? = null

        try {
            val customCoverPath = game.customCoverPath

            if (ContentHandler.isContentUri(customCoverPath)) {
                try {
                    contentUri = ContentHandler.unmangle(customCoverPath)
                } catch (_: FileNotFoundException) {
                    // Let contentUri remain null
                } catch (_: SecurityException) {
                    // Let contentUri remain null
                }
            } else {
                val cover = File(customCoverPath)
                if (cover.exists()) {
                    contentUri = getUriForFile(context, getFileProvider(context), cover)
                }
            }

            val resolvedContentUri = contentUri ?: (CoverHelper.buildGameTDBUrl(
                game, CoverHelper.getRegion(game)
            )).toUri()

            context.grantUriPermission(
                LEANBACK_PACKAGE,
                resolvedContentUri,
                android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
            contentUri = resolvedContentUri
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create banner")
            Log.e(TAG, e.message ?: "null")
        }

        return contentUri
    }

    /**
     * Needed since debug builds append '.debug' to the end of the package
     */
    private fun getFileProvider(context: Context): String {
        return context.packageName + ".filesprovider"
    }

    /**
     * Schedules syncing channels via a [JobScheduler].
     *
     * @param context for accessing the [JobScheduler].
     */
    @JvmStatic
    fun scheduleSyncingChannel(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return
        }

        val componentName = ComponentName(context, SyncChannelJobService::class.java)
        val builder = JobInfo.Builder(1, componentName)
        builder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)

        val scheduler = context.getSystemService(Context.JOB_SCHEDULER_SERVICE) as JobScheduler

        Log.d(TAG, "Scheduled channel creation.")
        scheduler.schedule(builder.build())
    }

    /**
     * Schedulers syncing programs for a channel. The scheduler will listen to a [Uri] for a
     * particular channel.
     *
     * @param context for accessing the [JobScheduler].
     * @param channelId for the channel to listen for changes.
     */
    @RequiresApi(Build.VERSION_CODES.O)
    @JvmStatic
    fun scheduleSyncingProgramsForChannel(context: Context, channelId: Long) {
        Log.d(TAG, "ProgramsRefresh job")
        val componentName = ComponentName(context, SyncProgramsJobService::class.java)
        val builder = JobInfo.Builder(getJobIdForChannelId(channelId), componentName)
        val triggerContentUri = JobInfo.TriggerContentUri(
            TvContractCompat.buildChannelUri(channelId),
            JobInfo.TriggerContentUri.FLAG_NOTIFY_FOR_DESCENDANTS
        )
        builder.addTriggerContentUri(triggerContentUri)
        builder.setTriggerContentMaxDelay(0L)
        builder.setTriggerContentUpdateDelay(0L)

        val bundle = PersistableBundle()
        bundle.putLong(TvContractCompat.EXTRA_CHANNEL_ID, channelId)
        builder.setExtras(bundle)

        val scheduler = context.getSystemService(Context.JOB_SCHEDULER_SERVICE) as JobScheduler
        scheduler.cancel(getJobIdForChannelId(channelId))
        scheduler.schedule(builder.build())
    }

    private fun getJobIdForChannelId(channelId: Long): Int {
        return (CHANNEL_JOB_ID_OFFSET + channelId).toInt()
    }

    /**
     * Generates all subscriptions for homescreen channels.
     */
    @JvmStatic
    fun createUniversalSubscriptions(context: Context): List<HomeScreenChannel> {
        return ArrayList(createPlatformSubscriptions(context))
    }

    private fun createPlatformSubscriptions(context: Context): List<HomeScreenChannel> {
        val subscriptions = ArrayList<HomeScreenChannel>()
        for (platformTab in PlatformTab.entries) {
            subscriptions.add(
                HomeScreenChannel(
                    context.getString(platformTab.headerName),
                    context.getString(platformTab.headerName),
                    AppLinkHelper.buildBrowseUri(platformTab)
                )
            )
        }
        return subscriptions
    }

    @JvmStatic
    fun isLeanback(context: Context): Boolean {
        return context.packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
    }
}
