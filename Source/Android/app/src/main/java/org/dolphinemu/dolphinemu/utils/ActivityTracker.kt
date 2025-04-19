package org.dolphinemu.dolphinemu.utils

import android.app.Activity
import android.app.Application.ActivityLifecycleCallbacks
import android.os.Bundle
import org.dolphinemu.dolphinemu.ui.main.MainView

class ActivityTracker : ActivityLifecycleCallbacks {
    private val resumedActivities = HashSet<Activity>()
    private var backgroundExecutionAllowed = false
    private var firstStart = true

    private fun isMainActivity(activity: Activity): Boolean {
      return activity is MainView
    }

    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
      if (isMainActivity(activity)) {
        firstStart = bundle == null
      }
    }

    override fun onActivityStarted(activity: Activity) {
      if (isMainActivity(activity)) {
        StartupHandler.reportStartToAnalytics(activity, firstStart)
        firstStart = false
      }
    }

    override fun onActivityResumed(activity: Activity) {
        resumedActivities.add(activity)
        if (!backgroundExecutionAllowed && !resumedActivities.isEmpty()) {
            backgroundExecutionAllowed = true
            setBackgroundExecutionAllowedNative(true)
        }
    }

    override fun onActivityPaused(activity: Activity) {
        resumedActivities.remove(activity)
        if (backgroundExecutionAllowed && resumedActivities.isEmpty()) {
            backgroundExecutionAllowed = false
            setBackgroundExecutionAllowedNative(false)
        }
    }

    override fun onActivityStopped(activity: Activity) {
      if (isMainActivity(activity)) {
        StartupHandler.updateSessionTimestamp(activity)
      }
    }

    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}

    override fun onActivityDestroyed(activity: Activity) {}

    companion object {
        @JvmStatic
        external fun setBackgroundExecutionAllowedNative(allowed: Boolean)
    }
}
