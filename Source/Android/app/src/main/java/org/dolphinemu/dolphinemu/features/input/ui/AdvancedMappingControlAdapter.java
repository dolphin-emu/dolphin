// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.databinding.ListItemAdvancedMappingControlBinding;

import java.util.function.Consumer;

public final class AdvancedMappingControlAdapter
        extends RecyclerView.Adapter<AdvancedMappingControlViewHolder>
{
  private final Consumer<String> mOnClickCallback;

  private String[] mControls = new String[0];

  public AdvancedMappingControlAdapter(Consumer<String> onClickCallback)
  {
    mOnClickCallback = onClickCallback;
  }

  @NonNull @Override
  public AdvancedMappingControlViewHolder onCreateViewHolder(@NonNull ViewGroup parent,
          int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());

    ListItemAdvancedMappingControlBinding binding =
            ListItemAdvancedMappingControlBinding.inflate(inflater);
    return new AdvancedMappingControlViewHolder(binding, mOnClickCallback);
  }

  @Override
  public void onBindViewHolder(@NonNull AdvancedMappingControlViewHolder holder, int position)
  {
    holder.bind(mControls[position]);
  }

  @Override
  public int getItemCount()
  {
    return mControls.length;
  }

  public void setControls(String[] controls)
  {
    mControls = controls;
    notifyDataSetChanged();
  }
}
