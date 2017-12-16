package org.dolphinemu.dolphinemu.ui.main;

import android.database.Cursor;

import org.dolphinemu.dolphinemu.ui.platform.Platform;

/**
 * Abstraction for the screen that shows on application launch.
 * Implementations will differ primarily to target touch-screen
 * or non-touch screen devices.
 */
public interface MainView
{
	/**
	 * Pass the view the native library's version string. Displaying
	 * it is optional.
	 *
	 * @param version A string pulled from native code.
	 */
	void setVersionString(String version);

	/**
	 * Tell the view to refresh its contents.
	 */
	void refresh();

	/**
	 * Tell the view to tell the currently displayed {@link android.support.v4.app.Fragment}
	 * to refresh the screenshot at the given position in its list of games.
	 *
	 * @param fragmentPosition An index corresponding to the list or grid of games.
	 */
	void refreshFragmentScreenshot(int fragmentPosition);


	void launchSettingsActivity(String menuTag);

	void launchFileListActivity();

	/**
	 * To be called when an asynchronous database read completes. Passes the
	 * result, in this case a {@link Cursor} to the view.
	 *
	 * @param platform Which platform to show games for.
	 * @param games A Cursor containing the games read from the database.
	 */
	void showGames(Platform platform, Cursor games);
}
