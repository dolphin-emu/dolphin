package org.dolphinemu.dolphinemu.ui.settings;


import android.app.Activity;
import android.app.Fragment;
import android.content.Context;
import android.support.design.widget.Snackbar;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.ui.FragmentContainer;
import org.dolphinemu.dolphinemu.utils.Log;

public abstract class BaseFragment extends Fragment
{
	private boolean mInjected = false;

	@Override
	public void onAttach(Context context)
	{
		super.onAttach(context);

		if (context instanceof Activity)
		{
			onAttachHelper((Activity) context);
		}
		else
		{
			// Shouldn't happen, but w/e
			Log.error("[BaseFragment] Attaching to a non-activity context not supported.");
		}
	}

	@Override
	public void onAttach(Activity activity)
	{
		super.onAttach(activity);
		onAttachHelper(activity);
	}

	private void onAttachHelper(Activity activity)
	{
		if (!mInjected)
		{
			inject();
			mInjected = true;
		}

		if (activity instanceof FragmentContainer)
		{
			String title = getTitle();
			if (title != null)
			{
				((FragmentContainer) activity).setActivityTitle(title);
			}
		}
	}

	public void showToastMessage(String message)
	{
		Toast.makeText(getActivity(), message, Toast.LENGTH_SHORT).show();
	}

	public void showErrorSnackbar(String message, View.OnClickListener action, int actionLabel)
	{
		Snackbar snackbar = Snackbar.make(getContentLayout(), message, Snackbar.LENGTH_LONG);

		if (action != null && actionLabel != 0)
		{
			snackbar.setAction(actionLabel, action);
		}

		snackbar.show();
	}

	protected abstract void inject();

	protected abstract FrameLayout getContentLayout();

	protected abstract String getTitle();
}