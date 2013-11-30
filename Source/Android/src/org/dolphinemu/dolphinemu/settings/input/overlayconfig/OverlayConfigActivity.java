/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input.overlayconfig;

import android.view.WindowManager;
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

		WindowManager wm = getWindowManager();

		// Initialize all of the buttons to add.
		final OverlayConfigButton buttonA = new OverlayConfigButton(wm, this, "gcpad_a", R.drawable.gcpad_a);
		final OverlayConfigButton buttonB = new OverlayConfigButton(wm, this, "gcpad_b", R.drawable.gcpad_b);
		final OverlayConfigButton buttonX = new OverlayConfigButton(wm, this, "gcpad_x", R.drawable.gcpad_x);
		final OverlayConfigButton buttonY = new OverlayConfigButton(wm, this, "gcpad_y", R.drawable.gcpad_y);
		final OverlayConfigButton buttonZ = new OverlayConfigButton(wm, this, "gcpad_z", R.drawable.gcpad_z);
		final OverlayConfigButton buttonS = new OverlayConfigButton(wm, this, "gcpad_start", R.drawable.gcpad_start);
		final OverlayConfigButton buttonL = new OverlayConfigButton(wm, this, "gcpad_l", R.drawable.gcpad_l);
		final OverlayConfigButton buttonR = new OverlayConfigButton(wm, this, "gcpad_r", R.drawable.gcpad_r);
		final OverlayConfigButton joystick = new OverlayConfigButton(wm, this, "gcpad_joystick_range", R.drawable.gcpad_joystick_range);

		// Add the buttons to the layout
		final RelativeLayout configLayout = new RelativeLayout(this);
		configLayout.addView(buttonA);
		configLayout.addView(buttonB);
		configLayout.addView(buttonX);
		configLayout.addView(buttonY);
		configLayout.addView(buttonZ);
		configLayout.addView(buttonS);
		configLayout.addView(buttonL);
		configLayout.addView(buttonR);
		configLayout.addView(joystick);

		// Now set the layout
		setContentView(configLayout);
	}
}
