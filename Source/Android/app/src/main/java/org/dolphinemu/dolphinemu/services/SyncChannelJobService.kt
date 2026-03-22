// SPDX-License-Identifier: GPL-2.0-or-later

@file:Suppress("DEPRECATION")

package org.dolphinemu.dolphinemu.services

import android.annotation.TargetApi
import android.app.job.JobParameters
import android.app.job.JobService
import android.content.ContentUris
import android.content.Context
import android.graphics.Bitmap
import android.media.tv.TvContract
import android.net.Uri
import android.os.AsyncTask
import android.os.Build
import android.util.Log
import androidx.tvprovider.media.tv.Channel
import androidx.tvprovider.media.tv.ChannelLogoUtils
import androidx.tvprovider.media.tv.TvContractCompat
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.HomeScreenChannel
import org.dolphinemu.dolphinemu.utils.TvUtil

class SyncChannelJobService : JobService() {
    private var syncChannelTask: SyncChannelTask? = null

    override fun onStartJob(jobParameters: JobParameters): Boolean {
        Log.d(TAG, "Starting channel creation job")
        syncChannelTask = object : SyncChannelTask(applicationContext) {
            override fun onPostExecute(success: Boolean?) {
                super.onPostExecute(success)
                jobFinished(jobParameters, success != true)
            }
        }
        syncChannelTask!!.execute()
        return true
    }

    override fun onStopJob(jobParameters: JobParameters): Boolean {
        syncChannelTask?.cancel(true)
        return true
    }

    private open class SyncChannelTask(private val context: Context) :
        AsyncTask<Void, Void, Boolean>() {
        /**
         * Setup channels
         */
        @TargetApi(Build.VERSION_CODES.O)
        override fun doInBackground(vararg voids: Void?): Boolean {
            val channels: List<Channel> = TvUtil.getAllChannels(context)
            val channelIds = ArrayList<Long>()

            // Checks if the default channels are added.
            // If not, create the channels
            if (channels.isNotEmpty()) {
                channels.forEach { channel -> channelIds.add(channel.id) }
            } else {
                for (subscription in TvUtil.createUniversalSubscriptions(context)) {
                    val channelId = createChannel(subscription)
                    channelIds.add(channelId)
                    subscription.channelId = channelId
                    // Only the first channel added can be browsable without user intervention.
                    TvContractCompat.requestChannelBrowsable(context, channelId)
                }
            }

            // Schedule triggers to update programs
            channelIds.forEach { channel ->
                TvUtil.scheduleSyncingProgramsForChannel(
                    context, channel
                )
            }
            // Update all channels
            TvUtil.updateAllChannels(context)
            return true
        }

        private fun createChannel(subscription: HomeScreenChannel): Long {
            var channelId = getChannelIdFromTvProvider(context, subscription)
            if (channelId != -1L) {
                return channelId
            }

            // Create the channel since it has not been added to the TV Provider.
            val appLinkIntentUri: Uri = subscription.appLinkIntentUri

            val builder = Channel.Builder()
            builder.setType(TvContractCompat.Channels.TYPE_PREVIEW)
                .setDisplayName(subscription.name).setDescription(subscription.description)
                .setAppLinkIntentUri(appLinkIntentUri)

            Log.d(TAG, "Creating channel: ${subscription.name}")
            val channelUrl = context.contentResolver.insert(
                TvContractCompat.Channels.CONTENT_URI, builder.build().toContentValues()
            )

            channelId = ContentUris.parseId(channelUrl!!)
            val bitmap: Bitmap = TvUtil.convertToBitmap(context, R.drawable.ic_dolphin_launcher)
            ChannelLogoUtils.storeChannelLogo(context, channelId, bitmap)

            return channelId
        }

        private fun getChannelIdFromTvProvider(
            context: Context, subscription: HomeScreenChannel
        ): Long {
            val cursor = context.contentResolver.query(
                TvContractCompat.Channels.CONTENT_URI, arrayOf(
                    TvContractCompat.Channels._ID, TvContract.Channels.COLUMN_DISPLAY_NAME
                ), null, null, null
            )
            if (cursor != null && cursor.moveToFirst()) {
                do {
                    val channel = Channel.fromCursor(cursor)
                    if (subscription.name == channel.displayName) {
                        Log.d(
                            TAG,
                            "Channel already exists. Returning channel " + channel.id + " from TV Provider."
                        )
                        return channel.id
                    }
                } while (cursor.moveToNext())
            }
            return -1L
        }
    }

    companion object {
        private const val TAG = "ChannelJobSvc"
    }
}
