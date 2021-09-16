// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class HeaderViewHolder extends CheatItemViewHolder
{
  private TextView mHeaderName;

  public HeaderViewHolder(@NonNull View itemView)
  {
    super(itemView);

    mHeaderName = itemView.findViewById(R.id.text_header_name);
  }

  public void bind(CheatsActivity activity, CheatItem item, int position)
  {
    mHeaderName.setText(item.getString());
  }
}
