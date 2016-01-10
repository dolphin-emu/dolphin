package org.dolphinemu.dolphinemu.ui.main;


import android.database.Cursor;

public interface MainView
{
	void setSubtitle(String subtitle);

	void refresh();

	void refreshFragmentScreenshot(int fragmentPosition);

	void launchSettingsActivity();

	void launchFileListActivity();

	void showGames(int platformIndex, Cursor games);
}
