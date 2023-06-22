// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.net.Uri;

import androidx.annotation.StringDef;

import org.dolphinemu.dolphinemu.ui.platform.Platform;

import java.util.List;

/**
 * Helps link home screen selection to a game.
 */
public class AppLinkHelper
{
  public static final String PLAY = "play";
  public static final String BROWSE = "browse";
  private static final String SCHEMA_URI_PREFIX = "dolphinemu://app/";
  private static final String URI_PLAY = SCHEMA_URI_PREFIX + PLAY;
  private static final String URI_VIEW = SCHEMA_URI_PREFIX + BROWSE;
  private static final int URI_INDEX_OPTION = 0;
  private static final int URI_INDEX_CHANNEL = 1;
  private static final int URI_INDEX_GAME = 2;

  public static Uri buildGameUri(long channelId, String gameId)
  {
    return Uri.parse(URI_PLAY)
            .buildUpon()
            .appendPath(String.valueOf(channelId))
            .appendPath(String.valueOf(gameId))
            .build();
  }

  public static Uri buildBrowseUri(Platform platform)
  {
    return Uri.parse(URI_VIEW).buildUpon().appendPath(platform.getIdString()).build();
  }

  public static AppLinkAction extractAction(Uri uri)
  {
    if (isGameUri(uri))
      return new PlayAction(extractChannelId(uri), extractGameId(uri));
    else if (isBrowseUri(uri))
      return new BrowseAction(extractSubscriptionName(uri));

    throw new IllegalArgumentException("No action found for uri " + uri);
  }

  private static boolean isGameUri(Uri uri)
  {
    if (uri.getPathSegments().isEmpty())
    {
      return false;
    }
    String option = uri.getPathSegments().get(URI_INDEX_OPTION);
    return PLAY.equals(option);
  }

  private static boolean isBrowseUri(Uri uri)
  {
    if (uri.getPathSegments().isEmpty())
      return false;

    String option = uri.getPathSegments().get(URI_INDEX_OPTION);
    return BROWSE.equals(option);
  }

  private static String extractSubscriptionName(Uri uri)
  {
    return extract(uri, URI_INDEX_CHANNEL);
  }

  private static long extractChannelId(Uri uri)
  {
    return extractLong(uri, URI_INDEX_CHANNEL);
  }

  private static String extractGameId(Uri uri)
  {
    return extract(uri, URI_INDEX_GAME);
  }

  private static long extractLong(Uri uri, int index)
  {
    return Long.parseLong(extract(uri, index));
  }

  private static String extract(Uri uri, int index)
  {
    List<String> pathSegments = uri.getPathSegments();
    if (pathSegments.isEmpty() || pathSegments.size() < index)
      return null;
    return pathSegments.get(index);
  }

  @StringDef({BROWSE, PLAY})
  public @interface ActionFlags
  {
  }

  /**
   * Action for deep linking.
   */
  public interface AppLinkAction
  {
    /**
     * Returns an string representation of the action.
     */
    @ActionFlags
    String getAction();
  }

  /**
   * Action when clicking the channel icon
   */
  public static class BrowseAction implements AppLinkAction
  {
    private final String mSubscriptionName;

    private BrowseAction(String subscriptionName)
    {
      this.mSubscriptionName = subscriptionName;
    }

    @Override
    public String getAction()
    {
      return BROWSE;
    }
  }

  /**
   * Action when clicking a program(game)
   */
  public static class PlayAction implements AppLinkAction
  {
    private final long channelId;
    private final String gameId;

    private PlayAction(long channelId, String gameId)
    {
      this.channelId = channelId;
      this.gameId = gameId;
    }

    public long getChannelId()
    {
      return channelId;
    }

    public String getGameId()
    {
      return gameId;
    }

    @Override
    public String getAction()
    {
      return PLAY;
    }
  }
}
