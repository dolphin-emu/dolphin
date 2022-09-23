// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.lifecycle.ViewModelProvider;

import org.dolphinemu.dolphinemu.databinding.ListItemCheatBinding;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatViewHolder extends CheatItemViewHolder
        implements View.OnClickListener, CompoundButton.OnCheckedChangeListener
{
  private final ListItemCheatBinding mBinding;

  private CheatsViewModel mViewModel;
  private Cheat mCheat;
  private int mPosition;

  public CheatViewHolder(@NonNull ListItemCheatBinding binding)
  {
    super(binding.getRoot());
    mBinding = binding;
  }

  public void bind(CheatsActivity activity, CheatItem item, int position)
  {
    mBinding.checkbox.setOnCheckedChangeListener(null);

    mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);
    mCheat = item.getCheat();
    mPosition = position;

    mBinding.textName.setText(mCheat.getName());
    mBinding.checkbox.setChecked(mCheat.getEnabled());

    mBinding.root.setOnClickListener(this);
    mBinding.checkbox.setOnCheckedChangeListener(this);
  }

  public void onClick(View root)
  {
    mViewModel.setSelectedCheat(mCheat, mPosition);
    mViewModel.openDetailsView();
  }

  public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
  {
    mCheat.setEnabled(isChecked);
    mViewModel.notifyCheatChanged(mPosition);
  }
}
