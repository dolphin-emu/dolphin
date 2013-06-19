/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package net.simonvt.menudrawer;

import android.view.animation.AnimationUtils;
import android.view.animation.Interpolator;

/**
 * This class encapsulates scrolling.  The duration of the scroll
 * can be passed in the constructor and specifies the maximum time that
 * the scrolling animation should take.  Past this time, the scrolling is
 * automatically moved to its final stage and computeScrollOffset()
 * will always return false to indicate that scrolling is over.
 */
public class FloatScroller {

    private float mStart;
    private float mFinal;

    private float mCurr;
    private long mStartTime;
    private int mDuration;
    private float mDurationReciprocal;
    private float mDeltaX;
    private boolean mFinished;
    private Interpolator mInterpolator;

    /**
     * Create a Scroller with the specified interpolator. If the interpolator is
     * null, the default (viscous) interpolator will be used. Specify whether or
     * not to support progressive "flywheel" behavior in flinging.
     */
    public FloatScroller(Interpolator interpolator) {
        mFinished = true;
        mInterpolator = interpolator;
    }

    /**
     * Returns whether the scroller has finished scrolling.
     *
     * @return True if the scroller has finished scrolling, false otherwise.
     */
    public final boolean isFinished() {
        return mFinished;
    }

    /**
     * Force the finished field to a particular value.
     *
     * @param finished The new finished value.
     */
    public final void forceFinished(boolean finished) {
        mFinished = finished;
    }

    /**
     * Returns how long the scroll event will take, in milliseconds.
     *
     * @return The duration of the scroll in milliseconds.
     */
    public final int getDuration() {
        return mDuration;
    }

    /**
     * Returns the current offset in the scroll.
     *
     * @return The new offset as an absolute distance from the origin.
     */
    public final float getCurr() {
        return mCurr;
    }

    /**
     * Returns the start offset in the scroll.
     *
     * @return The start offset as an absolute distance from the origin.
     */
    public final float getStart() {
        return mStart;
    }

    /**
     * Returns where the scroll will end. Valid only for "fling" scrolls.
     *
     * @return The final offset as an absolute distance from the origin.
     */
    public final float getFinal() {
        return mFinal;
    }

    public boolean computeScrollOffset() {
        if (mFinished) {
            return false;
        }

        int timePassed = (int) (AnimationUtils.currentAnimationTimeMillis() - mStartTime);

        if (timePassed < mDuration) {
            float x = timePassed * mDurationReciprocal;
            x = mInterpolator.getInterpolation(x);
            mCurr = mStart + x * mDeltaX;

        } else {
            mCurr = mFinal;
            mFinished = true;
        }
        return true;
    }

    public void startScroll(float start, float delta, int duration) {
        mFinished = false;
        mDuration = duration;
        mStartTime = AnimationUtils.currentAnimationTimeMillis();
        mStart = start;
        mFinal = start + delta;
        mDeltaX = delta;
        mDurationReciprocal = 1.0f / (float) mDuration;
    }

    /**
     * Stops the animation. Contrary to {@link #forceFinished(boolean)},
     * aborting the animating cause the scroller to move to the final x and y
     * position
     *
     * @see #forceFinished(boolean)
     */
    public void abortAnimation() {
        mCurr = mFinal;
        mFinished = true;
    }

    /**
     * Extend the scroll animation. This allows a running animation to scroll
     * further and longer, when used with {@link #setFinal(float)}.
     *
     * @param extend Additional time to scroll in milliseconds.
     * @see #setFinal(float)
     */
    public void extendDuration(int extend) {
        int passed = timePassed();
        mDuration = passed + extend;
        mDurationReciprocal = 1.0f / mDuration;
        mFinished = false;
    }

    /**
     * Returns the time elapsed since the beginning of the scrolling.
     *
     * @return The elapsed time in milliseconds.
     */
    public int timePassed() {
        return (int) (AnimationUtils.currentAnimationTimeMillis() - mStartTime);
    }

    public void setFinal(float newVal) {
        mFinal = newVal;
        mDeltaX = mFinal - mStart;
        mFinished = false;
    }
}
