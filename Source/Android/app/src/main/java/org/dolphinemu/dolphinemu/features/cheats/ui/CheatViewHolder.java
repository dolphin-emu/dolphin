// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;

public class CheatViewHolder extends ViewHolder
{
  private TextView mName;

  public CheatViewHolder(@NonNull View itemView)
  {
    super(itemView);

    mName = itemView.findViewById(R.id.text_name);
  }

  public void bind(Cheat item)
  {
    mName.setText(item.getName());
  }
}
