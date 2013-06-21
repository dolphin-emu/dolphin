package net.simonvt.menudrawer;

import android.view.animation.Interpolator;

/**
 * Interpolator which, when drawn from 0 to 1, looks like half a sine-wave. Used for smoother opening/closing when
 * peeking at the drawer.
 */
public class SinusoidalInterpolator implements Interpolator {

    @Override
    public float getInterpolation(float input) {
        return (float) (0.5f + 0.5f * Math.sin(input * Math.PI - Math.PI / 2.f));
    }
}
