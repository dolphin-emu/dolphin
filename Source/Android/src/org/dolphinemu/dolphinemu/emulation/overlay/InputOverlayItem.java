package org.dolphinemu.dolphinemu.emulation.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;

/**
 * Represents a drawable image for the {@link InputOverlay}
 */
public final class InputOverlayItem
{
	// The image as a BitmapDrawable
	private BitmapDrawable drawable;
	
	// Width and height of the underlying image.
	private int width;
	private int height;

	// X and Y coordinates to display this item at.
	private int x;
	private int y;

	// Image scale factor.
	private float scaleFactor = 1.0f;

	// Rectangle that we draw this item to.
	private Rect drawRect;

	/**
	 * Constructor.
	 * 
	 * @param res   Reference to the app resources for fetching display metrics.
	 * @param resId Resource ID of the {@link BitmapDrawable} to encapsulate.
	 */
	public InputOverlayItem(Resources res, int resId)
	{
		// Idiot-proof the constructor.
		if (res == null)
			throw new IllegalArgumentException("res cannot be null");

		// Everything is valid, decode the filename as a bitmap.
		drawable = (BitmapDrawable) res.getDrawable(resId);
		Bitmap image = drawable.getBitmap();

		// Set width/height
		width = image.getWidth();
		height = image.getHeight();

		// Initialize rectangle to zero width, height, x, and y.
		drawRect = new Rect();
	}

	/**
	 * Constructor
	 * 
	 * @param res   Reference to the app resources for fetching display metrics.
	 * @param resId Resource ID of the {@link BitmapDrawable} to encapsulate.
	 * @param x     X coordinate on the screen to place the control.
	 * @param y     Y coordinate on the screen to place the control.
	 */
	public InputOverlayItem(Resources res, int resId, int x, int y)
	{
		this(res, resId);

		setPosition(x, y);
	}

	/**
	 * Sets the position of this item on the screen.
	 * 
	 * @param x New x-coordinate for this image.
	 * @param y New y-coordinate for this image.
	 */
	public void setPosition(int x, int y)
	{
		this.x = x;
		this.y = y;

		drawRect.set(x, y, x + (int)(width * scaleFactor), y + (int)(height * scaleFactor));
		drawable.setBounds(drawRect);
	}

	/**
	 * Sets a new scaling factor for the current image.
	 * 
	 * @param scaleFactor The new scaling factor. Note that 1.0 is normal size.
	 */
	public void setScaleFactor(float scaleFactor)
	{
		this.scaleFactor = scaleFactor;

		// Adjust for the new scale factor.
		setPosition(x, y);
	}

	/**
	 * Draws this item to a given canvas.
	 * 
	 * @param canvas The canvas to draw this item to.
	 */
	public void draw(Canvas canvas)
	{
		drawable.draw(canvas);
	}
}
