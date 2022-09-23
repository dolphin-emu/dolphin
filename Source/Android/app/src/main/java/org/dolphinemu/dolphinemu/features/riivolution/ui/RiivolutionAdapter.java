// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.databinding.ListItemRiivolutionBinding;
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches;

import java.util.ArrayList;

public class RiivolutionAdapter extends RecyclerView.Adapter<RiivolutionViewHolder>
{
  private final Context mContext;
  private final RiivolutionPatches mPatches;
  private final ArrayList<RiivolutionItem> mItems = new ArrayList<>();

  public RiivolutionAdapter(Context context, RiivolutionPatches patches)
  {
    mContext = context;
    mPatches = patches;

    int discCount = mPatches.getDiscCount();
    for (int i = 0; i < discCount; i++)
    {
      mItems.add(new RiivolutionItem(i));

      int sectionCount = mPatches.getSectionCount(i);
      for (int j = 0; j < sectionCount; j++)
      {
        mItems.add(new RiivolutionItem(i, j));

        int optionCount = mPatches.getOptionCount(i, j);
        for (int k = 0; k < optionCount; k++)
        {
          mItems.add(new RiivolutionItem(i, j, k));
        }
      }
    }
  }

  @NonNull @Override
  public RiivolutionViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    ListItemRiivolutionBinding binding = ListItemRiivolutionBinding.inflate(inflater);
    return new RiivolutionViewHolder(binding.getRoot(), binding);
  }

  @Override
  public void onBindViewHolder(@NonNull RiivolutionViewHolder holder, int position)
  {
    holder.bind(mContext, mPatches, mItems.get(position));
  }

  @Override
  public int getItemCount()
  {
    return mItems.size();
  }
}
