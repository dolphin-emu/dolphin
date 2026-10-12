package org.dolphinemu.dolphinemu.model

import androidx.annotation.Keep
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.Observer

object VideoInterface {
    @Keep
    @JvmStatic
    external fun getTargetRefreshRate(): Double

    @Keep
    @JvmStatic
    external fun observeTargetRefreshRate(owner: LifecycleOwner, observer: Observer<Double>)
}
