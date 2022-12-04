package org.dolphinemu.dolphinemu.utils;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.graphics.Insets;

import com.google.android.material.appbar.AppBarLayout;

public class InsetsHelper
{
  public static final int THREE_BUTTON_NAVIGATION = 0;
  public static final int TWO_BUTTON_NAVIGATION = 1;
  public static final int GESTURE_NAVIGATION = 2;

  public static void insetAppBar(Insets insets, AppBarLayout appBarLayout)
  {
    ViewGroup.MarginLayoutParams mlpAppBar =
            (ViewGroup.MarginLayoutParams) appBarLayout.getLayoutParams();
    mlpAppBar.leftMargin = insets.left;
    mlpAppBar.topMargin = insets.top;
    mlpAppBar.rightMargin = insets.right;
    appBarLayout.setLayoutParams(mlpAppBar);
  }

  // Workaround for a bug on Android 13 that allows users to interact with UI behind the
  // navigation bar https://issuetracker.google.com/issues/248761842
  public static void applyNavbarWorkaround(int bottomInset, View workaroundView)
  {
    if (bottomInset > 0)
    {
      ViewGroup.LayoutParams lpWorkaround = workaroundView.getLayoutParams();
      lpWorkaround.height = bottomInset;
      workaroundView.setLayoutParams(lpWorkaround);
    }
  }

  public static int getSystemGestureType(Context context)
  {
    Resources resources = context.getResources();
    int resourceId = resources.getIdentifier("config_navBarInteractionMode", "integer", "android");
    if (resourceId != 0)
    {
      return resources.getInteger(resourceId);
    }
    return 0;
  }

  public static int getNavigationBarHeight(Context context)
  {
    Resources resources = context.getResources();
    int resourceId = resources.getIdentifier("navigation_bar_height", "dimen", "android");
    if (resourceId > 0)
    {
      return resources.getDimensionPixelSize(resourceId);
    }
    return 0;
  }

  // This is primarily intended to account for any navigation bar at the bottom of the screen
  public static int getBottomPaddingRequired(Activity activity)
  {
    Rect visibleFrame = new Rect();
    activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleFrame);
    return visibleFrame.bottom - visibleFrame.top -
            activity.getResources().getDisplayMetrics().heightPixels;
  }
}
