package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.ConfirmRunnable;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsFragmentView;

import androidx.appcompat.app.AlertDialog;

public final class ConfirmRunnableViewHolder extends SettingViewHolder
{
  private ConfirmRunnable mItem;
  private SettingsFragmentView mView;

  private Context mContext;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public ConfirmRunnableViewHolder(View itemView, SettingsAdapter adapter, Context context,
          SettingsFragmentView view)
  {
    super(itemView, adapter);

    mContext = context;
    mView = view;
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = (TextView) root.findViewById(R.id.text_setting_description);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (ConfirmRunnable) item;

    mTextSettingName.setText(item.getNameId());

    if (item.getDescriptionId() > 0)
    {
      mTextSettingDescription.setText(item.getDescriptionId());
    }
  }

  @Override
  public void onClick(View clicked)
  {
    String alertTitle = mContext.getString(mItem.getNameId());
    String alertText = mContext.getString(mItem.getAlertText());

    AlertDialog.Builder builder = new AlertDialog.Builder(mContext, R.style.DolphinDialogBase)
            .setTitle(alertTitle)
            .setMessage(alertText);

    builder
            .setPositiveButton("Yes", (dialog, whichButton) ->
            {
              mItem.getRunnable().run();

              if (mItem.getConfirmationText() > 0)
              {
                String confirmationText = mContext.getString(mItem.getConfirmationText());
                Toast.makeText(mContext, confirmationText, Toast.LENGTH_SHORT).show();
              }
              dialog.dismiss();

              // TODO: Remove finish and properly update dynamic settings descriptions.
              mView.getActivity().finish();
            })
            .setNegativeButton("No", (dialog, whichButton) ->
            {
              dialog.dismiss();
            });

    builder.show();
  }
}
