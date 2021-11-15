// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
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

    switch (viewType)
    {
      case RiivolutionViewHolder.TYPE_HEADER:
        View headerView = inflater.inflate(R.layout.list_item_riivolution_header, parent, false);
        return new RiivolutionViewHolder(headerView);
      case RiivolutionViewHolder.TYPE_OPTION:
        View optionView = inflater.inflate(R.layout.list_item_riivolution_option, parent, false);
        return new RiivolutionViewHolder(optionView);
      default:
        throw new UnsupportedOperationException();
    }
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

  @Override
  public int getItemViewType(int position)
  {
    return mItems.get(position).mOptionIndex != -1 ?
            RiivolutionViewHolder.TYPE_OPTION : RiivolutionViewHolder.TYPE_HEADER;
  }
}
