package org.dolphinemu.dolphinemu.utils

import android.app.Activity
import android.content.Context
import android.content.res.Resources
import android.graphics.Rect
import android.view.View
import android.view.ViewGroup
import androidx.core.graphics.Insets
import com.google.android.material.appbar.AppBarLayout

object InsetsHelper {
    const val THREE_BUTTON_NAVIGATION = 0
    const val TWO_BUTTON_NAVIGATION = 1
    const val GESTURE_NAVIGATION = 2

    @JvmStatic
    fun insetAppBar(insets: Insets, appBarLayout: AppBarLayout) {
        val mlpAppBar = appBarLayout.layoutParams as ViewGroup.MarginLayoutParams
        mlpAppBar.leftMargin = insets.left
        mlpAppBar.topMargin = insets.top
        mlpAppBar.rightMargin = insets.right
        appBarLayout.layoutParams = mlpAppBar
    }

    // Workaround for a bug on Android 13 that allows users to interact with UI behind the
    // navigation bar https://issuetracker.google.com/issues/248761842
    @JvmStatic
    fun applyNavbarWorkaround(bottomInset: Int, workaroundView: View) {
        if (bottomInset > 0) {
            val lpWorkaround = workaroundView.layoutParams
            lpWorkaround.height = bottomInset
            workaroundView.layoutParams = lpWorkaround
        }
    }

    @JvmStatic
    fun getSystemGestureType(context: Context): Int {
        val resources: Resources = context.resources
        val resourceId =
            resources.getIdentifier("config_navBarInteractionMode", "integer", "android")
        return if (resourceId != 0) {
            resources.getInteger(resourceId)
        } else {
            0
        }
    }

    @JvmStatic
    fun getNavigationBarHeight(context: Context): Int {
        val resources: Resources = context.resources
        val resourceId = resources.getIdentifier("navigation_bar_height", "dimen", "android")
        return if (resourceId > 0) {
            resources.getDimensionPixelSize(resourceId)
        } else {
            0
        }
    }

    // This is primarily intended to account for any navigation bar at the bottom of the screen
    @JvmStatic
    fun getBottomPaddingRequired(activity: Activity): Int {
        val visibleFrame = Rect()
        activity.window.decorView.getWindowVisibleDisplayFrame(visibleFrame)
        return visibleFrame.bottom - visibleFrame.top - activity.resources.displayMetrics.heightPixels
    }
}
