// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.PersistableBundle;
import android.util.Log;

import androidx.tvprovider.media.tv.Channel;
import androidx.tvprovider.media.tv.PreviewProgram;
import androidx.tvprovider.media.tv.TvContractCompat;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AppLinkHelper;
import org.dolphinemu.dolphinemu.utils.TvUtil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class SyncProgramsJobService extends JobService
{
  private static final String TAG = "SyncProgramsJobService";

  private SyncProgramsTask mSyncProgramsTask;

  @Override
  public boolean onStartJob(final JobParameters jobParameters)
  {
    Log.d(TAG, "onStartJob(): " + jobParameters);
    final long channelId = getChannelId(jobParameters);
    if (channelId == -1L)
    {
      Log.d(TAG, "Failed to find channel");
      return false;
    }

    mSyncProgramsTask =
            new SyncProgramsTask(getApplicationContext())
            {
              @Override
              protected void onPostExecute(Boolean finished)
              {
                super.onPostExecute(finished);
                mSyncProgramsTask = null;
                jobFinished(jobParameters, !finished);
              }
            };
    mSyncProgramsTask.execute(channelId);
    return true;
  }

  @Override
  public boolean onStopJob(JobParameters jobParameters)
  {
    if (mSyncProgramsTask != null)
    {
      mSyncProgramsTask.cancel(true);
    }
    return true;
  }

  private long getChannelId(JobParameters jobParameters)
  {
    PersistableBundle extras = jobParameters.getExtras();
    return extras.getLong(TvContractCompat.EXTRA_CHANNEL_ID, -1L);
  }

  private static class SyncProgramsTask extends AsyncTask<Long, Void, Boolean>
  {
    private Context context;
    private List<GameFile> updatePrograms;

    private SyncProgramsTask(Context context)
    {
      this.context = context;
      updatePrograms = new ArrayList<>();
    }

    /**
     * Determines which channel to update, get the game files for the channel,
     * then updates the list
     */
    @Override
    protected Boolean doInBackground(Long... channelIds)
    {
      List<Long> params = Arrays.asList(channelIds);
      if (!params.isEmpty())
      {
        for (Long channelId : params)
        {
          Channel channel = TvUtil.getChannelById(context, channelId);
          for (Platform platform : Platform.values())
          {
            if (channel != null &&
                    channel.getAppLinkIntentUri().equals(AppLinkHelper.buildBrowseUri(platform)))
            {
              getGamesByPlatform(platform);
              syncPrograms(channelId);
            }
          }
        }
      }
      return true;
    }

    private void getGamesByPlatform(Platform platform)
    {
      updatePrograms = GameFileCacheManager.getGameFilesForPlatform(platform);
    }

    private void syncPrograms(long channelId)
    {
      Log.d(TAG, "Sync programs for channel: " + channelId);
      deletePrograms(channelId);
      createPrograms(channelId);
    }

    private void createPrograms(long channelId)
    {
      for (GameFile game : updatePrograms)
      {
        PreviewProgram previewProgram = buildProgram(channelId, game);

        context.getContentResolver()
                .insert(
                        TvContractCompat.PreviewPrograms.CONTENT_URI,
                        previewProgram.toContentValues());
      }
    }

    private void deletePrograms(long channelId)
    {
      context.getContentResolver().delete(
              TvContractCompat.buildPreviewProgramsUriForChannel(channelId),
              null,
              null);
    }

    private PreviewProgram buildProgram(long channelId, GameFile game)
    {
      Uri appLinkUri = AppLinkHelper.buildGameUri(channelId, game.getGameId());
      Uri banner = TvUtil.buildBanner(game, context);
      if (banner == null)
        banner = TvUtil.getUriToResource(context, R.drawable.banner_tv);

      PreviewProgram.Builder builder = new PreviewProgram.Builder();
      builder.setChannelId(channelId)
              .setType(TvContractCompat.PreviewProgramColumns.TYPE_GAME)
              .setTitle(game.getTitle())
              .setDescription(game.getDescription())
              .setPosterArtUri(banner)
              .setPosterArtAspectRatio(TvContractCompat.PreviewPrograms.ASPECT_RATIO_2_3)
              .setIntentUri(appLinkUri);
      return builder.build();
    }
  }
}
