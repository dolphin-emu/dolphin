/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.activities;

import android.app.Activity;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.widget.RelativeLayout;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.overlay.OverlayConfigButton;

/**
 * {@link Activity} used for configuring the input overlay.
 */
public final class OverlayConfigActivity extends AppCompatActivity
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Initialize all of the buttons to add.
		final OverlayConfigButton buttonA = new OverlayConfigButton(this, "gcpad_a", R.drawable.gcpad_a);
		final OverlayConfigButton buttonB = new OverlayConfigButton(this, "gcpad_b", R.drawable.gcpad_b);
		final OverlayConfigButton buttonX = new OverlayConfigButton(this, "gcpad_x", R.drawable.gcpad_x);
		final OverlayConfigButton buttonY = new OverlayConfigButton(this, "gcpad_y", R.drawable.gcpad_y);
		final OverlayConfigButton buttonZ = new OverlayConfigButton(this, "gcpad_z", R.drawable.gcpad_z);
		final OverlayConfigButton buttonS = new OverlayConfigButton(this, "gcpad_start", R.drawable.gcpad_start);
		final OverlayConfigButton buttonL = new OverlayConfigButton(this, "gcpad_l", R.drawable.gcpad_l);
		final OverlayConfigButton buttonR = new OverlayConfigButton(this, "gcpad_r", R.drawable.gcpad_r);
		final OverlayConfigButton joystick = new OverlayConfigButton(this, "gcpad_joystick_range", R.drawable.gcpad_joystick_range);

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
