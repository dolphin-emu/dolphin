package org.dolphinemu.dolphinemu.utils;

import android.view.View;
import android.view.ViewPropertyAnimator;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;

public final class Animations
{
	private static final Interpolator DECELERATOR = new DecelerateInterpolator();
	private static final Interpolator ACCELERATOR = new AccelerateInterpolator();

	private Animations()
	{
	}

	public static ViewPropertyAnimator fadeViewOutToRight(View view)
	{
		return view.animate()
				.withLayer()
				.setDuration(200)
				.setInterpolator(ACCELERATOR)
				.alpha(0.0f)
				.translationX(view.getWidth());
	}

	public static ViewPropertyAnimator fadeViewOutToLeft(View view)
	{
		return view.animate()
				.withLayer()
				.setDuration(200)
				.setInterpolator(ACCELERATOR)
				.alpha(0.0f)
				.translationX(-view.getWidth());
	}

	public static ViewPropertyAnimator fadeViewInFromLeft(View view)
	{

		view.setVisibility(View.VISIBLE);

		view.setTranslationX(-view.getWidth());
		view.setAlpha(0.0f);

		return view.animate()
				.withLayer()
				.setDuration(300)
				.setInterpolator(DECELERATOR)
				.alpha(1.0f)
				.translationX(0.0f);
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
