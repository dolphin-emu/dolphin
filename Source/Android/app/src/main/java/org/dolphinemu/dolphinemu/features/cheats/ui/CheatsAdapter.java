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

public class CheatsAdapter extends RecyclerView.Adapter<CheatViewHolder>
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
  public CheatViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    View view = inflater.inflate(R.layout.list_item_cheat, parent, false);
    return new CheatViewHolder(view);
  }

  @Override
  public void onBindViewHolder(@NonNull CheatViewHolder holder, int position)
  {
    holder.bind(mViewModel, getItemAt(position), position);
  }

  @Override
  public int getItemCount()
  {
    return mViewModel.getARCheats().length + mViewModel.getGeckoCheats().length +
            mViewModel.getPatchCheats().length;
  }

  private Cheat getItemAt(int position)
  {
    Cheat[] patchCheats = mViewModel.getPatchCheats();
    if (position < patchCheats.length)
    {
      return patchCheats[position];
    }
    position -= patchCheats.length;

    Cheat[] arCheats = mViewModel.getARCheats();
    if (position < arCheats.length)
    {
      return arCheats[position];
    }
    position -= arCheats.length;

    Cheat[] geckoCheats = mViewModel.getGeckoCheats();
    if (position < geckoCheats.length)
    {
      return geckoCheats[position];
    }
    position -= geckoCheats.length;

    throw new IndexOutOfBoundsException();
  }
}
