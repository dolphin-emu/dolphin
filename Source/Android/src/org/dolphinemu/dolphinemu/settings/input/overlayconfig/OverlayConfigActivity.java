/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input.overlayconfig;

import org.dolphinemu.dolphinemu.R;

import android.app.Activity;
import android.os.Bundle;
import android.widget.RelativeLayout;

/**
 * {@link Activity} used for configuring the input overlay.
 */
public final class OverlayConfigActivity extends Activity
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Initialize all of the buttons to add.
		final OverlayConfigButton buttonA = new OverlayConfigButton(this, "button_a", R.drawable.button_a);
		final OverlayConfigButton buttonB = new OverlayConfigButton(this, "button_b", R.drawable.button_b);
		final OverlayConfigButton buttonS = new OverlayConfigButton(this, "button_start", R.drawable.button_start);

		// Add the buttons to the layout
		final RelativeLayout configLayout = new RelativeLayout(this);
		configLayout.addView(buttonA);
		configLayout.addView(buttonB);
		configLayout.addView(buttonS);

		// Now set the layout
		setContentView(configLayout);
	}
}
