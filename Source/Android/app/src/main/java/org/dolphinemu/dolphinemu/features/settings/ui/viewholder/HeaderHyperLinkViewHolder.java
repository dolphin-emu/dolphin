// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui.viewholder;

import android.text.method.LinkMovementMethod;

import androidx.annotation.NonNull;

import com.google.android.material.color.MaterialColors;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;

public final class HeaderHyperLinkViewHolder extends HeaderViewHolder
{
  private final ListItemHeaderBinding mBinding;

  public HeaderHyperLinkViewHolder(@NonNull ListItemHeaderBinding binding, SettingsAdapter adapter)
  {
    super(binding, adapter);
    mBinding = binding;
    itemView.setOnClickListener(null);
  }

  @Override
  public void bind(@NonNull SettingsItem item)
  {
    super.bind(item);

    mBinding.textHeaderName.setMovementMethod(LinkMovementMethod.getInstance());
    mBinding.textHeaderName.setLinkTextColor(
            MaterialColors.getColor(itemView, R.attr.colorTertiary));
  }
}
