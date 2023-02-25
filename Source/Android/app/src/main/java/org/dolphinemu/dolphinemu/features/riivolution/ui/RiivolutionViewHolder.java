// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemRiivolutionBinding;
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches;

public class RiivolutionViewHolder extends RecyclerView.ViewHolder
        implements AdapterView.OnItemClickListener
{
  private final ListItemRiivolutionBinding mBinding;

  private RiivolutionPatches mPatches;
  private RiivolutionItem mItem;

  public RiivolutionViewHolder(@NonNull View itemView, ListItemRiivolutionBinding binding)
  {
    super(itemView);
    mBinding = binding;
  }

  public void bind(Context context, RiivolutionPatches patches, RiivolutionItem item)
  {
    // TODO: Remove workaround for text filtering issue in material components when fixed
    // https://github.com/material-components/material-components-android/issues/1464
    mBinding.dropdownChoice.setSaveEnabled(false);

    String text;
    if (item.mOptionIndex != -1)
    {
      mBinding.textName.setVisibility(View.GONE);
      text = patches.getOptionName(item.mDiscIndex, item.mSectionIndex, item.mOptionIndex);
      mBinding.layoutChoice.setHint(text);
    }
    else if (item.mSectionIndex != -1)
    {
      mBinding.textName.setTextAppearance(context, R.style.TextAppearance_AppCompat_Medium);
      mBinding.layoutChoice.setVisibility(View.GONE);
      text = patches.getSectionName(item.mDiscIndex, item.mSectionIndex);
    }
    else
    {
      mBinding.layoutChoice.setVisibility(View.GONE);
      text = patches.getDiscName(item.mDiscIndex);
    }
    mBinding.textName.setText(text);

    if (item.mOptionIndex != -1)
    {
      mPatches = patches;
      mItem = item;

      ArrayAdapter<String> adapter = new ArrayAdapter<>(context,
              R.layout.support_simple_spinner_dropdown_item);

      int choiceCount =
              patches.getChoiceCount(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex);
      adapter.add(context.getString(R.string.riivolution_disabled));
      for (int i = 0; i < choiceCount; i++)
      {
        adapter.add(patches.getChoiceName(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex,
                i));
      }

      mBinding.dropdownChoice.setAdapter(adapter);
      mBinding.dropdownChoice.setText(adapter.getItem(
                      patches.getSelectedChoice(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex)),
              false);
      mBinding.dropdownChoice.setOnItemClickListener(this);
    }
  }

  @Override
  public void onItemClick(AdapterView<?> parent, View view, int position, long id)
  {
    mPatches.setSelectedChoice(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex, position);
  }
}
