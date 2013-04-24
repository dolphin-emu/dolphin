package net.simonvt.menudrawer.samples;

import android.os.Bundle;
import android.view.Window;

public class ActionBarOverlaySample extends WindowSample {

    @Override
    public void onCreate(Bundle inState) {
        requestWindowFeature(Window.FEATURE_ACTION_BAR_OVERLAY);
        super.onCreate(inState);
    }
}
