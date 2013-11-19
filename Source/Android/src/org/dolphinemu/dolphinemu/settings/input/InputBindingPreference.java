package org.dolphinemu.dolphinemu.settings.input;

import org.dolphinemu.dolphinemu.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.Preference;
import android.util.AttributeSet;

/**
 * {@link Preference} subclass that represents a preference
 * used for assigning a key bind.
 */
public final class InputBindingPreference extends Preference
{
	/**
	 * Constructor that is called when inflating an InputBindingPreference from XML.
	 * 
	 * @param context The current {@link Context}.
	 * @param attrs   The attributes of the XML tag that is inflating the preference.
	 */
	public InputBindingPreference(Context context, AttributeSet attrs)
	{
		super(context, attrs);
	}

	@Override
	protected void onClick()
	{
		// Begin the creation of the input alert.
		final MotionAlertDialog dialog = new MotionAlertDialog(getContext(), this);

		// Set the cancel button.
		dialog.setButton(AlertDialog.BUTTON_NEGATIVE, getContext().getString(R.string.cancel), new AlertDialog.OnClickListener()
		{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
						// Do nothing. Just makes the cancel button show up.
				}
		});

		// Set the title and description message.
		dialog.setTitle(R.string.input_binding);
		dialog.setMessage(String.format(getContext().getString(R.string.input_binding_descrip), getTitle()));

		// Don't allow the dialog to close when a user taps
		// outside of it. They must press cancel or provide an input.
		dialog.setCanceledOnTouchOutside(false);

		// Everything is set, show the dialog.
		dialog.show();
	}
}
