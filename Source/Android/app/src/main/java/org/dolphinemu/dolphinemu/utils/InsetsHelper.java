package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.widget.NestedScrollView;
import androidx.recyclerview.widget.RecyclerView;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.appbar.AppBarLayout;
import com.google.android.material.color.MaterialColors;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import org.dolphinemu.dolphinemu.R;

public class InsetsHelper
{
  public static final int FAB_INSET = 16;
  public static final int EXTRA_NAV_INSET = 32;

  public static final int THREE_BUTTON_NAVIGATION = 0;
  public static final int TWO_BUTTON_NAVIGATION = 1;
  public static final int GESTURE_NAVIGATION = 2;

  public static int dpToPx(Context context, int dp)
  {
    return (int) (dp * ((float) context.getResources().getDisplayMetrics().densityDpi /
            DisplayMetrics.DENSITY_DEFAULT) + 0.5f);
  }

  public static void setUpAppBarWithScrollView(AppCompatActivity activity,
          AppBarLayout appBarLayout, NestedScrollView nestedScrollView, View workaroundView)
  {
    ViewCompat.setOnApplyWindowInsetsListener(appBarLayout, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      insetAppBar(insets, appBarLayout);

      nestedScrollView.setPadding(insets.left, 0, insets.right, insets.bottom);

      applyWorkaround(insets.bottom, workaroundView);

      ThemeHelper.setNavigationBarColor(activity,
              MaterialColors.getColor(appBarLayout, R.attr.colorSurface));

      return windowInsets;
    });
  }

  public static void setUpList(Context context, RecyclerView recyclerView)
  {
    ViewCompat.setOnApplyWindowInsetsListener(recyclerView, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      v.setPadding(0, 0, 0, insets.bottom + dpToPx(context, EXTRA_NAV_INSET));

      return windowInsets;
    });
  }

  public static void setUpMainLayout(AppCompatActivity activity, AppBarLayout appBarLayout,
          FloatingActionButton fab, ViewPager viewPager, View workaroundView)
  {
    ViewCompat.setOnApplyWindowInsetsListener(appBarLayout, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      insetAppBar(insets, appBarLayout);

      ViewGroup.MarginLayoutParams mlpFab = (ViewGroup.MarginLayoutParams) fab.getLayoutParams();
      int fabPadding =
              InsetsHelper.dpToPx(activity.getApplicationContext(), FAB_INSET);
      mlpFab.leftMargin = insets.left + fabPadding;
      mlpFab.bottomMargin = insets.bottom + fabPadding;
      mlpFab.rightMargin = insets.right + fabPadding;
      fab.setLayoutParams(mlpFab);

      viewPager.setPadding(insets.left, 0, insets.right, 0);

      applyWorkaround(insets.bottom, workaroundView);

      ThemeHelper.setNavigationBarColor(activity,
              MaterialColors.getColor(appBarLayout, R.attr.colorSurface));

      return windowInsets;
    });
  }

  public static void setUpSettingsLayout(AppCompatActivity activity,
          AppBarLayout appBarLayout, FrameLayout frameLayout, View workaroundView)
  {
    ViewCompat.setOnApplyWindowInsetsListener(appBarLayout, (v, windowInsets) ->
    {
      Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());

      insetAppBar(insets, appBarLayout);

      frameLayout.setPadding(insets.left, 0, insets.right, 0);

      applyWorkaround(insets.bottom, workaroundView);

      ThemeHelper.setNavigationBarColor(activity,
              MaterialColors.getColor(appBarLayout, R.attr.colorSurface));

      return windowInsets;
    });
  }

  public static void setUpCheatsLayout(AppCompatActivity activity, AppBarLayout appBarLayout,
          SlidingPaneLayout slidingPaneLayout, View cheatDetails, View workaroundView)
  {
    ViewCompat.setOnApplyWindowInsetsListener(appBarLayout, (v, windowInsets) ->
    {
      Insets barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      Insets keyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());

      insetAppBar(barInsets, appBarLayout);

      slidingPaneLayout.setPadding(barInsets.left, barInsets.top, barInsets.right, 0);

      if (keyboardInsets.bottom > 0)
      {
        cheatDetails.setPadding(0, 0, 0, keyboardInsets.bottom);
      }
      else
      {
        cheatDetails.setPadding(0, 0, 0, barInsets.bottom);
      }

      applyWorkaround(barInsets.bottom, workaroundView);

      ThemeHelper.setNavigationBarColor(activity,
              MaterialColors.getColor(appBarLayout, R.attr.colorSurface));

      return windowInsets;
    });
  }

  private static void insetAppBar(Insets insets, AppBarLayout appBarLayout)
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
  private static void applyWorkaround(int bottomInset, View workaroundView)
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
}
