package org.dolphinemu.dolphinemu.utils;

import android.view.View;
import android.view.ViewPropertyAnimator;

public final class Animations
{
	private Animations()
	{
	}

	public static ViewPropertyAnimator fadeViewIn(View view)
	{
		view.setVisibility(View.VISIBLE);

		return view.animate()
				.withLayer()
				.setDuration(100)
				.alpha(1.0f);
	}

	public static ViewPropertyAnimator fadeViewOut(View view)
	{
		return view.animate()
				.withLayer()
				.setDuration(300)
				.alpha(0.0f);
	}
}
