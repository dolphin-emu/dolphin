// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.lifecycle.LifecycleOwner;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;

public class CheatsAdapter extends RecyclerView.Adapter<CheatItemViewHolder>
{
  private final CheatsViewModel mViewModel;

  public CheatsAdapter(LifecycleOwner owner, CheatsViewModel viewModel)
  {
    mViewModel = viewModel;

    mViewModel.getCheatChangedEvent().observe(owner, (position) ->
    {
      if (position != null)
        notifyItemChanged(position);
    });
  }

  @NonNull
  @Override
  public CheatItemViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());

    switch (viewType)
    {
      case CheatItem.TYPE_CHEAT:
        View cheatView = inflater.inflate(R.layout.list_item_cheat, parent, false);
        return new CheatViewHolder(cheatView);
      case CheatItem.TYPE_HEADER:
        View headerView = inflater.inflate(R.layout.list_item_header, parent, false);
        return new HeaderViewHolder(headerView);
      default:
        throw new UnsupportedOperationException();
    }
  }

  @Override
  public void onBindViewHolder(@NonNull CheatItemViewHolder holder, int position)
  {
    holder.bind(mViewModel, getItemAt(position), position);
  }

  @Override
  public int getItemCount()
  {
    return mViewModel.getARCheats().length + mViewModel.getGeckoCheats().length +
            mViewModel.getPatchCheats().length + 3;
  }

  @Override
  public int getItemViewType(int position)
  {
    return getItemAt(position).getType();
  }

  private CheatItem getItemAt(int position)
  {
    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_patch);
    position -= 1;

    Cheat[] patchCheats = mViewModel.getPatchCheats();
    if (position < patchCheats.length)
      return new CheatItem(patchCheats[position]);
    position -= patchCheats.length;

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_ar);
    position -= 1;

    Cheat[] arCheats = mViewModel.getARCheats();
    if (position < arCheats.length)
      return new CheatItem(arCheats[position]);
    position -= arCheats.length;

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_gecko);
    position -= 1;

    Cheat[] geckoCheats = mViewModel.getGeckoCheats();
    if (position < geckoCheats.length)
      return new CheatItem(geckoCheats[position]);
    position -= geckoCheats.length;

    throw new IndexOutOfBoundsException();
  }
}
