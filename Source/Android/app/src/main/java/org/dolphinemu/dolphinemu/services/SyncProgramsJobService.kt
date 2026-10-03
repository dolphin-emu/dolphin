// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services

import android.app.job.JobParameters
import android.app.job.JobService
import android.content.ContentValues
import android.content.Context
import android.media.tv.TvContract
import android.util.Log
import androidx.tvprovider.media.tv.TvContractCompat
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.ui.platform.PlatformTab
import org.dolphinemu.dolphinemu.utils.AppLinkHelper.buildBrowseUri
import org.dolphinemu.dolphinemu.utils.AppLinkHelper.buildGameUri
import org.dolphinemu.dolphinemu.utils.TvUtil

class SyncProgramsJobService : JobService() {
    private val syncProgramsScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var syncProgramsJob: Job? = null

    override fun onStartJob(jobParameters: JobParameters): Boolean {
        Log.d(TAG, "onStartJob(): $jobParameters")
        val channelId = getChannelId(jobParameters)
        if (channelId == -1L) {
            Log.d(TAG, "Failed to find channel")
            return false
        }

        syncProgramsJob?.cancel()
        syncProgramsJob = syncProgramsScope.launch {
            val finished = try {
                withContext(Dispatchers.IO) {
                    SyncProgramsTask(applicationContext).run(channelId)
                }
            } catch (_: CancellationException) {
                return@launch
            } catch (e: RuntimeException) {
                Log.e(TAG, "Failed to sync programs", e)
                false
            }

            syncProgramsJob = null
            jobFinished(jobParameters, !finished)
        }

        return true
    }

    override fun onStopJob(jobParameters: JobParameters): Boolean {
        syncProgramsJob?.cancel()
        syncProgramsJob = null
        return true
    }

    override fun onDestroy() {
        syncProgramsScope.cancel()
        super.onDestroy()
    }

    private fun getChannelId(jobParameters: JobParameters): Long {
        val extras = jobParameters.extras
        return extras.getLong(TvContractCompat.EXTRA_CHANNEL_ID, -1L)
    }

    private class SyncProgramsTask(private val context: Context) {
        private var updatePrograms: List<GameFile> = emptyList()

        /**
         * Determines which channel to update, get the game files for the channel,
         * then updates the list
         */
        suspend fun run(vararg channelIds: Long): Boolean {
            for (channelId in channelIds) {
                currentCoroutineContext().ensureActive()

                val channel = TvUtil.getChannelById(context, channelId)
                for (platformTab in PlatformTab.entries) {
                    if (channel?.appLinkIntentUri == buildBrowseUri(platformTab)) {
                        updatePrograms =
                            GameFileCacheManager.getGameFilesForPlatformTab(platformTab)
                        if (!syncPrograms(channelId)) {
                            return false
                        }
                    }
                }
            }

            return true
        }

        private suspend fun syncPrograms(channelId: Long): Boolean {
            Log.d(TAG, "Sync programs for channel: $channelId")
            deletePrograms(channelId)
            return createPrograms(channelId)
        }

        private suspend fun createPrograms(channelId: Long): Boolean {
            for (game in updatePrograms) {
                currentCoroutineContext().ensureActive()

                val previewProgram = buildProgram(channelId, game)
                context.contentResolver.insert(
                    TvContractCompat.PreviewPrograms.CONTENT_URI, previewProgram
                )
            }

            return true
        }

        private fun deletePrograms(channelId: Long) {
            context.contentResolver.delete(
                TvContractCompat.buildPreviewProgramsUriForChannel(channelId), null, null
            )
        }

        private fun buildProgram(channelId: Long, game: GameFile): ContentValues {
            val appLinkUri = buildGameUri(channelId, game.getGameId())
            var banner = TvUtil.buildBanner(game, context)
            if (banner == null) {
                banner = TvUtil.getUriToResource(context, R.drawable.banner_tv)
            }

            val values = ContentValues()
            values.put(TvContractCompat.PreviewPrograms.COLUMN_CHANNEL_ID, channelId)
            values.put(
                TvContractCompat.PreviewPrograms.COLUMN_TYPE,
                TvContractCompat.PreviewProgramColumns.TYPE_GAME
            )
            values.put(TvContract.Programs.COLUMN_TITLE, game.getTitle())
            values.put(TvContract.Programs.COLUMN_SHORT_DESCRIPTION, game.getDescription())
            values.put(TvContract.Programs.COLUMN_POSTER_ART_URI, banner.toString())
            values.put(
                TvContractCompat.PreviewPrograms.COLUMN_POSTER_ART_ASPECT_RATIO,
                TvContractCompat.PreviewPrograms.ASPECT_RATIO_2_3
            )
            values.put(TvContractCompat.PreviewPrograms.COLUMN_INTENT_URI, appLinkUri.toString())
            return values
        }
    }

    companion object {
        private const val TAG = "SyncProgramsJobService"
    }
}
