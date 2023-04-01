package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;

public final class Pixels
{
	private Pixels()
	{
	}

	public static float convertDpToPx(float original, Context context)
	{
		Resources resources = context.getResources();
		DisplayMetrics metrics = resources.getDisplayMetrics();

		return original * metrics.density;
	}

	public static float convertPxToDp(float original, Context context)
	{
		Resources resources = context.getResources();
		DisplayMetrics metrics = resources.getDisplayMetrics();

		return original / metrics.density;
	}
}
