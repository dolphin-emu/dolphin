/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableButton extends BitmapDrawable
{
	// The ID identifying what type of button this Drawable represents.
	private int buttonType;

	/**
	 * Constructor
	 * 
	 * @param res         {@link Resources} instance.
	 * @param bitmap      {@link Bitmap} to use with this Drawable.
	 * @param buttonType  Identifier for this type of button.
	 */
	public InputOverlayDrawableButton(Resources res, Bitmap bitmap, int buttonType)
	{
		super(res, bitmap);

		this.buttonType = buttonType;
	}

	/**
	 * Gets this InputOverlayDrawableButton's button ID.
	 * 
	 * @return this InputOverlayDrawableButton's button ID.
	 */
	public int getId()
	{
		return buttonType;
	}
}
