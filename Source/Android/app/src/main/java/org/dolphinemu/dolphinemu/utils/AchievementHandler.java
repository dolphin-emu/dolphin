package org.dolphinemu.dolphinemu.utils;

import android.util.Log;

public class AchievementHandler  {

	public static void UnlockAchievement(String achieve)
	{
		Log.i("DolphinEmu", "Unlocking achievement " + achieve);
	}
	public static void IncrementAchievement(String achieve, int amount)
	{
		Log.i("DolphinEmu", "Incrementing achievement " + achieve + " by " + amount);
	}
}
