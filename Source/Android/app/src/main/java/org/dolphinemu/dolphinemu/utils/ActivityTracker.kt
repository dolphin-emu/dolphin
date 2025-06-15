package org.dolphinemu.dolphinemu.utils

import android.app.Activity
import android.app.Application.ActivityLifecycleCallbacks
import android.os.Bundle
import org.dolphinemu.dolphinemu.ui.main.MainView

class ActivityTracker : ActivityLifecycleCallbacks {
    private val resumedActivities = HashSet<Activity>()
    private var backgroundExecutionAllowed = false
    private var firstStart = true
    var currentActivity : Activity? = null
        private set

    private fun isMainActivity(activity: Activity): Boolean {
        return activity is MainView
    }

    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
        currentActivity = activity
        if (isMainActivity(activity)) {
            firstStart = bundle == null
        }
    }

    override fun onActivityStarted(activity: Activity) {
        StartupHandler.reportStartToAnalytics(activity.applicationContext, firstStart)
        firstStart = false
    }

    override fun onActivityResumed(activity: Activity) {
        currentActivity = activity
        resumedActivities.add(activity)
        if (!backgroundExecutionAllowed && !resumedActivities.isEmpty()) {
            backgroundExecutionAllowed = true
            setBackgroundExecutionAllowedNative(true)
        }
    }

    override fun onActivityPaused(activity: Activity) {
        if (currentActivity === activity) {
            currentActivity = null
        }
        resumedActivities.remove(activity)
        if (backgroundExecutionAllowed && resumedActivities.isEmpty()) {
            backgroundExecutionAllowed = false
            setBackgroundExecutionAllowedNative(false)
        }
    }

    override fun onActivityStopped(activity: Activity) {
        if (isMainActivity(activity)) {
            StartupHandler.updateSessionTimestamp(activity.applicationContext)
        }
    }

    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}

    override fun onActivityDestroyed(activity: Activity) {}

    companion object {
        @JvmStatic
        external fun setBackgroundExecutionAllowedNative(allowed: Boolean)
    }
}
