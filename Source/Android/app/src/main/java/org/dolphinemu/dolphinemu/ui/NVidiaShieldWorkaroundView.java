/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

/**
 * Work around a bug with the nVidia Shield.
 */
public final class NVidiaShieldWorkaroundView extends View
{
	public NVidiaShieldWorkaroundView(Context context, AttributeSet attrs)
	{
		super(context, attrs);

		// Setting this seems to workaround the bug
		setWillNotDraw(false);
	}
}
