// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.view.RunRunnable;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class RunRunnableViewHolder extends SettingViewHolder
{
  private RunRunnable mItem;

  private final Context mContext;

  private TextView mTextSettingName;
  private TextView mTextSettingDescription;

  public RunRunnableViewHolder(View itemView, SettingsAdapter adapter, Context context)
  {
    super(itemView, adapter);

    mContext = context;
  }

  @Override
  protected void findViews(View root)
  {
    mTextSettingName = root.findViewById(R.id.text_setting_name);
    mTextSettingDescription = root.findViewById(R.id.text_setting_description);
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (RunRunnable) item;

    mTextSettingName.setText(item.getName());
    mTextSettingDescription.setText(item.getDescription());
  }

  @Override
  public void onClick(View clicked)
  {
    int alertTextID = mItem.getAlertText();

    if (alertTextID > 0)
    {
      AlertDialog.Builder builder = new AlertDialog.Builder(mContext)
              .setTitle(mItem.getName())
              .setMessage(alertTextID);

      builder
              .setPositiveButton(R.string.ok, (dialog, whichButton) ->
              {
                runRunnable();
                dialog.dismiss();
              })
              .setNegativeButton(R.string.cancel, (dialog, whichButton) -> dialog.dismiss());

      builder.show();
    }
    else
    {
      runRunnable();
    }
  }

  @Nullable @Override
  protected SettingsItem getItem()
  {
    return mItem;
  }

  private void runRunnable()
  {
    mItem.getRunnable().run();

    if (mItem.getToastTextAfterRun() > 0)
    {
      Toast.makeText(mContext, mContext.getString(mItem.getToastTextAfterRun()), Toast.LENGTH_SHORT)
              .show();
    }
  }
}
