package net.simonvt.menudrawer;

import android.view.animation.Interpolator;

public class PeekInterpolator implements Interpolator {

    private static final String TAG = "PeekInterpolator";

    private static final SinusoidalInterpolator SINUSOIDAL_INTERPOLATOR = new SinusoidalInterpolator();

    @Override
    public float getInterpolation(float input) {
        float result;

        if (input < 1.f / 3.f) {
            result = SINUSOIDAL_INTERPOLATOR.getInterpolation(input * 3);

        } else if (input > 2.f / 3.f) {
            final float val = ((input + 1.f / 3.f) - 1.f) * 3.f;
            result = 1.f - SINUSOIDAL_INTERPOLATOR.getInterpolation(val);

        } else {
            result = 1.f;
        }

        return result;
    }
}
