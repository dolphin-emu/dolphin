/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.custom;

import android.content.Context;
import android.preference.ListPreference;
import android.util.AttributeSet;

/**
 * A {@link ListPreference} that updates its summary upon it's selection being changed.
 */
public final class UpdatingListPreference extends ListPreference
{
	/**
	 * Constructor
	 * 
	 * @param context the current {@link Context}.
	 */
	public UpdatingListPreference(Context context, AttributeSet attribs)
	{
		super(context, attribs);
	}

	@Override
	public void onDialogClosed(boolean positiveResult)
	{
		super.onDialogClosed(positiveResult);

		// If an entry was selected
		if (positiveResult)
		{
			setSummary(getEntry());
		}
	}
}
