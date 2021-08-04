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
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatViewHolder extends ViewHolder
        implements View.OnClickListener, CompoundButton.OnCheckedChangeListener
{
  private final View mRoot;
  private final TextView mName;
  private final CheckBox mCheckbox;

  private CheatsViewModel mViewModel;
  private Cheat mCheat;

  public CheatViewHolder(@NonNull View itemView)
  {
    super(itemView);

    mRoot = itemView.findViewById(R.id.root);
    mName = itemView.findViewById(R.id.text_name);
    mCheckbox = itemView.findViewById(R.id.checkbox);
  }

  public void bind(CheatsViewModel viewModel, Cheat item)
  {
    mCheckbox.setOnCheckedChangeListener(null);

    mName.setText(item.getName());
    mCheckbox.setChecked(item.getEnabled());

    mViewModel = viewModel;
    mCheat = item;

    mRoot.setOnClickListener(this);
    mCheckbox.setOnCheckedChangeListener(this);
  }

  public void onClick(View root)
  {
    mViewModel.setSelectedCheat(mCheat);
  }

  public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
  {
    mCheat.setEnabled(isChecked);
  }
}
