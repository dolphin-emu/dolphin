// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;

public class CheatViewHolder extends ViewHolder implements CompoundButton.OnCheckedChangeListener
{
  private final TextView mName;
  private final CheckBox mCheckbox;

  private Cheat mCheat;

  public CheatViewHolder(@NonNull View itemView)
  {
    super(itemView);

    mName = itemView.findViewById(R.id.text_name);
    mCheckbox = itemView.findViewById(R.id.checkbox);
  }

  public void bind(Cheat item)
  {
    mCheckbox.setOnCheckedChangeListener(null);

    mName.setText(item.getName());
    mCheckbox.setChecked(item.getEnabled());

    mCheat = item;

    mCheckbox.setOnCheckedChangeListener(this);
  }

  public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
  {
    mCheat.setEnabled(isChecked);
  }
}
