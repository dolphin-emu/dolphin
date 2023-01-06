// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.PersistableBundle;
import android.util.Log;

import androidx.annotation.AnyRes;
import androidx.annotation.NonNull;
import androidx.tvprovider.media.tv.Channel;
import androidx.tvprovider.media.tv.TvContractCompat;

import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.model.HomeScreenChannel;
import org.dolphinemu.dolphinemu.services.SyncChannelJobService;
import org.dolphinemu.dolphinemu.services.SyncProgramsJobService;
import org.dolphinemu.dolphinemu.ui.platform.Platform;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;

import static android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION;
import static androidx.core.content.FileProvider.getUriForFile;

/**
 * Assists in TV related services, e.g., home screen channels
 */
public class TvUtil
{
  private static final String TAG = "TvUtil";
  private static final long CHANNEL_JOB_ID_OFFSET = 1000;

  private static final String[] CHANNELS_PROJECTION = {
          TvContractCompat.Channels._ID,
          TvContractCompat.Channels.COLUMN_DISPLAY_NAME,
          TvContractCompat.Channels.COLUMN_BROWSABLE,
          TvContractCompat.Channels.COLUMN_APP_LINK_INTENT_URI
  };
  private static final String LEANBACK_PACKAGE = "com.google.android.tvlauncher";

  public static int getNumberOfChannels(Context context)
  {
    Cursor cursor =
            context.getContentResolver()
                    .query(
                            TvContractCompat.Channels.CONTENT_URI,
                            CHANNELS_PROJECTION,
                            null,
                            null,
                            null);
    return cursor != null ? cursor.getCount() : 0;
  }

  public static List<Channel> getAllChannels(Context context)
  {
    List<Channel> channels = new ArrayList<>();
    Cursor cursor =
            context.getContentResolver()
                    .query(
                            TvContractCompat.Channels.CONTENT_URI,
                            CHANNELS_PROJECTION,
                            null,
                            null,
                            null);
    if (cursor != null && cursor.moveToFirst())
    {
      do
      {
        channels.add(Channel.fromCursor(cursor));
      }
      while (cursor.moveToNext());
    }
    return channels;
  }

  public static Channel getChannelById(Context context, long channelId)
  {
    for (Channel channel : getAllChannels(context))
    {
      if (channel.getId() == channelId)
      {
        return channel;
      }
    }
    return null;
  }

  /**
   * Updates all Leanback homescreen channels
   */
  public static void updateAllChannels(Context context)
  {
    if (Build.VERSION.SDK_INT < 26)
      return;
    for (Channel channel : getAllChannels(context))
    {
      context.getContentResolver()
              .update(
                      TvContractCompat.buildChannelUri(channel.getId()),
                      channel.toContentValues(),
                      null,
                      null);
    }
  }

  public static Uri getUriToResource(Context context, @AnyRes int resId)
          throws Resources.NotFoundException
  {
    Resources res = context.getResources();
    return Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
            "://" + res.getResourcePackageName(resId)
            + '/' + res.getResourceTypeName(resId)
            + '/' + res.getResourceEntryName(resId));
  }

  /**
   * Converts a resource into a {@link Bitmap}. If the resource is a vector drawable, it will be
   * drawn into a new Bitmap. Otherwise the {@link BitmapFactory} will decode the resource.
   */
  @NonNull
  public static Bitmap convertToBitmap(Context context, int resourceId)
  {
    Drawable drawable = context.getDrawable(resourceId);
    if (drawable instanceof VectorDrawable)
    {
      Bitmap bitmap =
              Bitmap.createBitmap(
                      drawable.getIntrinsicWidth(),
                      drawable.getIntrinsicHeight(),
                      Bitmap.Config.ARGB_8888);
      Canvas canvas = new Canvas(bitmap);
      drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
      drawable.draw(canvas);
      return bitmap;
    }
    return BitmapFactory.decodeResource(context.getResources(), resourceId);
  }

  /**
   * Leanback launcher requires a uri for poster art so we create a contentUri and
   * pass that to LEANBACK_PACKAGE
   */
  public static Uri buildBanner(GameFile game, Context context)
  {
    Uri contentUri = null;
    File cover;

    try
    {
      String customCoverPath = game.getCustomCoverPath();

      if (ContentHandler.isContentUri(customCoverPath))
      {
        try
        {
          contentUri = ContentHandler.unmangle(customCoverPath);
        }
        catch (FileNotFoundException | SecurityException ignored)
        {
          // Let contentUri remain null
        }
      }
      else
      {
        if ((cover = new File(customCoverPath)).exists())
        {
          contentUri = getUriForFile(context, getFileProvider(context), cover);
        }
      }

      if (contentUri == null)
      {
        contentUri = Uri.parse(CoverHelper.buildGameTDBUrl(game, CoverHelper.getRegion(game)));
      }

      context.grantUriPermission(LEANBACK_PACKAGE, contentUri, FLAG_GRANT_READ_URI_PERMISSION);
    }
    catch (Exception e)
    {
      Log.e(TAG, "Failed to create banner");
      Log.e(TAG, e.getMessage());
    }

    return contentUri;
  }

  /**
   * Needed since debug builds append '.debug' to the end of the package
   */
  private static String getFileProvider(Context context)
  {
    return context.getPackageName() + ".filesprovider";
  }

  /**
   * Schedules syncing channels via a {@link JobScheduler}.
   *
   * @param context for accessing the {@link JobScheduler}.
   */
  public static void scheduleSyncingChannel(Context context)
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
      return;

    ComponentName componentName = new ComponentName(context, SyncChannelJobService.class);
    JobInfo.Builder builder = new JobInfo.Builder(1, componentName);
    builder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY);

    JobScheduler scheduler =
            (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);

    Log.d(TAG, "Scheduled channel creation.");
    scheduler.schedule(builder.build());
  }

  /**
   * Schedulers syncing programs for a channel. The scheduler will listen to a {@link Uri} for a
   * particular channel.
   *
   * @param context   for accessing the {@link JobScheduler}.
   * @param channelId for the channel to listen for changes.
   */
  @TargetApi(Build.VERSION_CODES.O)
  public static void scheduleSyncingProgramsForChannel(Context context, long channelId)
  {
    Log.d(TAG, "ProgramsRefresh job");
    ComponentName componentName = new ComponentName(context, SyncProgramsJobService.class);
    JobInfo.Builder builder =
            new JobInfo.Builder(getJobIdForChannelId(channelId), componentName);
    JobInfo.TriggerContentUri triggerContentUri =
            new JobInfo.TriggerContentUri(
                    TvContractCompat.buildChannelUri(channelId),
                    JobInfo.TriggerContentUri.FLAG_NOTIFY_FOR_DESCENDANTS);
    builder.addTriggerContentUri(triggerContentUri);
    builder.setTriggerContentMaxDelay(0L);
    builder.setTriggerContentUpdateDelay(0L);

    PersistableBundle bundle = new PersistableBundle();
    bundle.putLong(TvContractCompat.EXTRA_CHANNEL_ID, channelId);
    builder.setExtras(bundle);

    JobScheduler scheduler =
            (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
    scheduler.cancel(getJobIdForChannelId(channelId));
    scheduler.schedule(builder.build());
  }

  private static int getJobIdForChannelId(long channelId)
  {
    return (int) (CHANNEL_JOB_ID_OFFSET + channelId);
  }

  /**
   * Generates all subscriptions for homescreen channels.
   */
  public static List<HomeScreenChannel> createUniversalSubscriptions(Context context)
  {
    return new ArrayList<>(createPlatformSubscriptions(context));
  }

  private static List<HomeScreenChannel> createPlatformSubscriptions(Context context)
  {
    List<HomeScreenChannel> subs = new ArrayList<>();
    for (Platform platform : Platform.values())
    {
      subs.add(new HomeScreenChannel(
              context.getString(platform.getHeaderName()),
              context.getString(platform.getHeaderName()),
              AppLinkHelper.buildBrowseUri(platform)));
    }
    return subs;
  }

  public static Boolean isLeanback(Context context)
  {
    return (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK));
  }
}
