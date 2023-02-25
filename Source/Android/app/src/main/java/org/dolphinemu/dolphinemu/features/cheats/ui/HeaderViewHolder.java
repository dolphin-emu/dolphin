// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding;

public class HeaderViewHolder extends CheatItemViewHolder
{
  private TextView mHeaderName;

  public HeaderViewHolder(@NonNull ListItemHeaderBinding binding)
  {
    super(binding.getRoot());

    mHeaderName = binding.textHeaderName;
  }

  public void bind(CheatsActivity activity, CheatItem item, int position)
  {
    mHeaderName.setText(item.getString());
  }
}
