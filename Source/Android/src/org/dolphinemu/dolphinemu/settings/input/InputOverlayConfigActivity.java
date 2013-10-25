/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.settings.input;

import org.dolphinemu.dolphinemu.R;

import android.app.Activity;
import android.os.Bundle;

/**
 * {@link Activity} used for configuring the input overlay.
 */
public final class InputOverlayConfigActivity extends Activity
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Set the initial layout.
		setContentView(R.layout.input_overlay_config_layout);
	}
}
