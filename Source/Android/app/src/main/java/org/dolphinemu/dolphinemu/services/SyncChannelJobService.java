// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.services;

import android.annotation.TargetApi;
import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.media.tv.TvContract;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.util.Log;

import androidx.tvprovider.media.tv.Channel;
import androidx.tvprovider.media.tv.ChannelLogoUtils;
import androidx.tvprovider.media.tv.TvContractCompat;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.HomeScreenChannel;
import org.dolphinemu.dolphinemu.utils.TvUtil;

import java.util.ArrayList;
import java.util.List;

public class SyncChannelJobService extends JobService
{
  private static final String TAG = "ChannelJobSvc";

  private SyncChannelTask mSyncChannelTask;

  @Override
  public boolean onStartJob(final JobParameters jobParameters)
  {
    Log.d(TAG, "Starting channel creation job");
    mSyncChannelTask =
            new SyncChannelTask(getApplicationContext())
            {
              @Override
              protected void onPostExecute(Boolean success)
              {
                super.onPostExecute(success);
                jobFinished(jobParameters, !success);
              }
            };
    mSyncChannelTask.execute();
    return true;
  }

  @Override
  public boolean onStopJob(JobParameters jobParameters)
  {
    if (mSyncChannelTask != null)
    {
      mSyncChannelTask.cancel(true);
    }
    return true;
  }

  private static class SyncChannelTask extends AsyncTask<Void, Void, Boolean>
  {
    private Context context;

    SyncChannelTask(Context context)
    {
      this.context = context;
    }

    /**
     * Setup channels
     */
    @TargetApi(Build.VERSION_CODES.O)
    @Override
    protected Boolean doInBackground(Void... voids)
    {
      List<HomeScreenChannel> subscriptions;
      List<Channel> channels = TvUtil.getAllChannels(context);
      List<Long> channelIds = new ArrayList<>();
      // Checks if the default channels are added.
      // If not, create the channels
      if (!channels.isEmpty())
      {
        channels.forEach(channel -> channelIds.add(channel.getId()));
      }
      else
      {
        subscriptions = TvUtil.createUniversalSubscriptions(context);
        for (HomeScreenChannel subscription : subscriptions)
        {
          long channelId = createChannel(subscription);
          channelIds.add(channelId);
          subscription.setChannelId(channelId);
          // Only the first channel added can be browsable without user intervention.
          TvContractCompat.requestChannelBrowsable(context, channelId);
        }
      }
      // Schedule triggers to update programs
      channelIds.forEach(channel -> TvUtil.scheduleSyncingProgramsForChannel(context, channel));
      // Update all channels
      TvUtil.updateAllChannels(context);
      return true;
    }

    private long createChannel(HomeScreenChannel subscription)
    {
      long channelId = getChannelIdFromTvProvider(context, subscription);
      if (channelId != -1L)
      {
        return channelId;
      }

      // Create the channel since it has not been added to the TV Provider.
      Uri appLinkIntentUri = subscription.getAppLinkIntentUri();

      Channel.Builder builder = new Channel.Builder();
      builder.setType(TvContractCompat.Channels.TYPE_PREVIEW)
              .setDisplayName(subscription.getName())
              .setDescription(subscription.getDescription())
              .setAppLinkIntentUri(appLinkIntentUri);

      Log.d(TAG, "Creating channel: " + subscription.getName());
      Uri channelUrl =
              context.getContentResolver()
                      .insert(
                              TvContractCompat.Channels.CONTENT_URI,
                              builder.build().toContentValues());

      channelId = ContentUris.parseId(channelUrl);
      Bitmap bitmap = TvUtil.convertToBitmap(context, R.drawable.ic_dolphin_launcher);
      ChannelLogoUtils.storeChannelLogo(context, channelId, bitmap);

      return channelId;
    }

    private long getChannelIdFromTvProvider(Context context, HomeScreenChannel subscription)
    {
      Cursor cursor =
              context.getContentResolver().query(
                      TvContractCompat.Channels.CONTENT_URI,
                      new String[]{
                              TvContractCompat.Channels._ID,
                              TvContract.Channels.COLUMN_DISPLAY_NAME
                      },
                      null,
                      null,
                      null);
      if (cursor != null && cursor.moveToFirst())
      {
        do
        {
          Channel channel = Channel.fromCursor(cursor);
          if (subscription.getName().equals(channel.getDisplayName()))
          {
            Log.d(
                    TAG,
                    "Channel already exists. Returning channel "
                            + channel.getId()
                            + " from TV Provider.");
            return channel.getId();
          }
        }
        while (cursor.moveToNext());
      }
      return -1L;
    }
  }
}
