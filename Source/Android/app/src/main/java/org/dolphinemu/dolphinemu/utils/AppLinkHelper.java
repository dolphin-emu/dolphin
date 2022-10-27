// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.net.Uri;

import androidx.annotation.StringDef;

import org.dolphinemu.dolphinemu.model.GameFile;

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
  private static final int URI_INDEX_GAME = 1;

  public static Uri buildGameUri(GameFile game)
  {
    return Uri.parse(URI_PLAY)
            .buildUpon()
            .appendPath(String.valueOf(game.getPlatform()))
            .appendPath(String.valueOf(game.getGameId()))
            .build();
  }

  public static Uri buildBrowseUri()
  {
    return Uri.parse(URI_VIEW)
            .buildUpon()
            .build();
  }

  public static AppLinkAction extractAction(Uri uri)
  {
    if (isGameUri(uri))
      return new PlayAction(extractGameId(uri));
    else if (isBrowseUri(uri))
      return new BrowseAction();

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

  private static String extractGameId(Uri uri)
  {
    return extract(uri, URI_INDEX_GAME);
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
    private BrowseAction()
    {
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
    private final String gameId;

    private PlayAction(String gameId)
    {
      this.gameId = gameId;
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
