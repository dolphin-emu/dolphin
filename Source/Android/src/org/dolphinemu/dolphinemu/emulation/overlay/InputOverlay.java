package org.dolphinemu.dolphinemu.emulation.overlay;

import java.util.HashSet;
import java.util.Set;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;

/**
 * Draws the interactive input overlay on top of the
 * {@link NativeGLSurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener
{
	private final Set<InputOverlayItem> overlayItems = new HashSet<InputOverlayItem>();

	/**
	 * Constructor
	 * 
	 * @param context The current {@link Context}.
	 * @param attrs   {@link AttributeSet} for parsing XML attributes.
	 */
	public InputOverlay(Context context, AttributeSet attrs)
	{
		super(context, attrs);

		// Force draw
		setWillNotDraw(false);

		// Request focus for the overlay so it has priority on presses.
		requestFocus();
	}

	@Override
	public boolean onTouch(View v, MotionEvent event)
	{
		switch (event.getAction())
		{
			case MotionEvent.ACTION_DOWN:
			{
				// TODO: Handle down presses.
				return true;
			}
		}

		return false;
	}

	@Override
	public void onDraw(Canvas canvas)
	{
		super.onDraw(canvas);

		for (InputOverlayItem item : overlayItems)
		{
			item.draw(canvas);
		}
	}
}
