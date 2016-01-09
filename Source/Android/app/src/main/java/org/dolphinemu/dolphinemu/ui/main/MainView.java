package org.dolphinemu.dolphinemu.ui.main;


public interface MainView
{
	void setSubtitle(String subtitle);

	void refresh();

	void refreshFragmentScreenshot(int fragmentPosition);

	void launchSettingsActivity();

	void launchFileListActivity();
}
