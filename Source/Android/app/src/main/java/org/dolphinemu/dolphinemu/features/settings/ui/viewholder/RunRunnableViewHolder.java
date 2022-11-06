// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.content.Context;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.RunRunnable;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class RunRunnableViewHolder extends SettingViewHolder
{
  private RunRunnable mItem;

  private final Context mContext;

  private final ListItemSettingBinding mBinding;

  public RunRunnableViewHolder(@NonNull ListItemSettingBinding binding, SettingsAdapter adapter,
          Context context)
  {
    super(binding.getRoot(), adapter);
    mBinding = binding;
    mContext = context;
  }

  @Override
  public void bind(SettingsItem item)
  {
    mItem = (RunRunnable) item;

    mBinding.textSettingName.setText(item.getName());
    mBinding.textSettingDescription.setText(item.getDescription());

    setStyle(mBinding.textSettingName, mItem);
  }

  @Override
  public void onClick(View clicked)
  {
    if (!mItem.isEditable())
    {
      showNotRuntimeEditableError();
      return;
    }

    int alertTextID = mItem.getAlertText();

    if (alertTextID > 0)
    {
      new MaterialAlertDialogBuilder(mContext)
              .setTitle(mItem.getName())
              .setMessage(alertTextID)
              .setPositiveButton(R.string.ok, (dialog, whichButton) ->
              {
                runRunnable();
                dialog.dismiss();
              })
              .setNegativeButton(R.string.cancel, (dialog, whichButton) -> dialog.dismiss())
              .show();
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
