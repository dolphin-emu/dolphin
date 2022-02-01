// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches;

public class RiivolutionViewHolder extends RecyclerView.ViewHolder
        implements AdapterView.OnItemSelectedListener
{
  public static final int TYPE_HEADER = 0;
  public static final int TYPE_OPTION = 1;

  private final TextView mTextView;
  private final Spinner mSpinner;

  private RiivolutionPatches mPatches;
  private RiivolutionItem mItem;

  public RiivolutionViewHolder(@NonNull View itemView)
  {
    super(itemView);

    mTextView = itemView.findViewById(R.id.text_name);
    mSpinner = itemView.findViewById(R.id.spinner_choice);
  }

  public void bind(Context context, RiivolutionPatches patches, RiivolutionItem item)
  {
    String text;
    if (item.mOptionIndex != -1)
      text = patches.getOptionName(item.mDiscIndex, item.mSectionIndex, item.mOptionIndex);
    else if (item.mSectionIndex != -1)
      text = patches.getSectionName(item.mDiscIndex, item.mSectionIndex);
    else
      text = patches.getDiscName(item.mDiscIndex);
    mTextView.setText(text);

    if (item.mOptionIndex != -1)
    {
      mPatches = patches;
      mItem = item;

      ArrayAdapter<String> adapter = new ArrayAdapter<>(context,
              R.layout.list_item_riivolution_header);

      int choiceCount = patches.getChoiceCount(mItem.mDiscIndex, mItem.mSectionIndex,
              mItem.mOptionIndex);
      adapter.add(context.getString(R.string.riivolution_disabled));
      for (int i = 0; i < choiceCount; i++)
      {
        adapter.add(patches.getChoiceName(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex,
                i));
      }

      mSpinner.setAdapter(adapter);
      mSpinner.setSelection(patches.getSelectedChoice(mItem.mDiscIndex, mItem.mSectionIndex,
              mItem.mOptionIndex));
      mSpinner.setOnItemSelectedListener(this);
    }
  }

  @Override
  public void onItemSelected(AdapterView<?> parent, View view, int position, long id)
  {
    mPatches.setSelectedChoice(mItem.mDiscIndex, mItem.mSectionIndex, mItem.mOptionIndex, position);
  }

  @Override
  public void onNothingSelected(AdapterView<?> parent)
  {
  }
}
