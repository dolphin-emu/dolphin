// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding;

import java.util.function.Consumer;

public class AdvancedMappingControlViewHolder extends RecyclerView.ViewHolder
{
  private final ListItemAdvancedMappingControlBinding mBinding;

  private String mName;

  public AdvancedMappingControlViewHolder(@NonNull ListItemAdvancedMappingControlBinding binding,
          Consumer<String> onClickCallback)
  {
    super(binding.getRoot());

    mBinding = binding;

    binding.getRoot().setOnClickListener(view -> onClickCallback.accept(mName));
  }

  public void bind(String name)
  {
    mName = name;

    mBinding.textName.setText(name);
  }
}
