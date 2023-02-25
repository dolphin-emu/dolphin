// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemCheatBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding;
import org.dolphinemu.dolphinemu.features.cheats.model.ARCheat;
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel;
import org.dolphinemu.dolphinemu.features.cheats.model.GeckoCheat;
import org.dolphinemu.dolphinemu.features.cheats.model.GraphicsMod;
import org.dolphinemu.dolphinemu.features.cheats.model.PatchCheat;

import java.util.ArrayList;

public class CheatsAdapter extends RecyclerView.Adapter<CheatItemViewHolder>
{
  private final CheatsActivity mActivity;
  private final CheatsViewModel mViewModel;

  public CheatsAdapter(CheatsActivity activity, CheatsViewModel viewModel)
  {
    mActivity = activity;
    mViewModel = viewModel;

    mViewModel.getCheatAddedEvent().observe(activity, (position) ->
    {
      if (position != null)
        notifyItemInserted(position);
    });

    mViewModel.getCheatChangedEvent().observe(activity, (position) ->
    {
      if (position != null)
        notifyItemChanged(position);
    });

    mViewModel.getCheatDeletedEvent().observe(activity, (position) ->
    {
      if (position != null)
        notifyItemRemoved(position);
    });

    mViewModel.getGeckoCheatsDownloadedEvent().observe(activity, (cheatsAdded) ->
    {
      if (cheatsAdded != null)
      {
        int positionEnd = getItemCount() - 2;  // Skip "Add Gecko Code" and "Download Gecko Codes"
        int positionStart = positionEnd - cheatsAdded;
        notifyItemRangeInserted(positionStart, cheatsAdded);
      }
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
        ListItemCheatBinding listItemCheatBinding = ListItemCheatBinding.inflate(inflater);
        addViewListeners(listItemCheatBinding.getRoot());
        return new CheatViewHolder(listItemCheatBinding);
      case CheatItem.TYPE_HEADER:
        ListItemHeaderBinding listItemHeaderBinding = ListItemHeaderBinding.inflate(inflater);
        addViewListeners(listItemHeaderBinding.getRoot());
        return new HeaderViewHolder(listItemHeaderBinding);
      case CheatItem.TYPE_ACTION:
        ListItemSubmenuBinding listItemSubmenuBinding = ListItemSubmenuBinding.inflate(inflater);
        addViewListeners(listItemSubmenuBinding.getRoot());
        return new ActionViewHolder(listItemSubmenuBinding);
      default:
        throw new UnsupportedOperationException();
    }
  }

  @Override
  public void onBindViewHolder(@NonNull CheatItemViewHolder holder, int position)
  {
    holder.bind(mActivity, getItemAt(position), position);
  }

  @Override
  public int getItemCount()
  {
    return mViewModel.getGraphicsMods().size() + mViewModel.getPatchCheats().size() +
            mViewModel.getARCheats().size() + mViewModel.getGeckoCheats().size() + 8;
  }

  @Override
  public int getItemViewType(int position)
  {
    return getItemAt(position).getType();
  }

  private void addViewListeners(View view)
  {
    CheatsActivity.setOnFocusChangeListenerRecursively(view,
            (v, hasFocus) -> mActivity.onListViewFocusChange(hasFocus));
  }

  private CheatItem getItemAt(int position)
  {
    // Graphics mods

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_graphics_mod);
    position -= 1;

    ArrayList<GraphicsMod> graphicsMods = mViewModel.getGraphicsMods();
    if (position < graphicsMods.size())
      return new CheatItem(graphicsMods.get(position));
    position -= graphicsMods.size();

    // Patches

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_patch);
    position -= 1;

    ArrayList<PatchCheat> patchCheats = mViewModel.getPatchCheats();
    if (position < patchCheats.size())
      return new CheatItem(patchCheats.get(position));
    position -= patchCheats.size();

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_patch);
    position -= 1;

    // AR codes

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_ar);
    position -= 1;

    ArrayList<ARCheat> arCheats = mViewModel.getARCheats();
    if (position < arCheats.size())
      return new CheatItem(arCheats.get(position));
    position -= arCheats.size();

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_ar);
    position -= 1;

    // Gecko codes

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_HEADER, R.string.cheats_header_gecko);
    position -= 1;

    ArrayList<GeckoCheat> geckoCheats = mViewModel.getGeckoCheats();
    if (position < geckoCheats.size())
      return new CheatItem(geckoCheats.get(position));
    position -= geckoCheats.size();

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_add_gecko);
    position -= 1;

    if (position == 0)
      return new CheatItem(CheatItem.TYPE_ACTION, R.string.cheats_download_gecko);

    throw new IndexOutOfBoundsException();
  }
}
