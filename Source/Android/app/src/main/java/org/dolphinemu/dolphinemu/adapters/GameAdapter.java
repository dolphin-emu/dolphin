// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.CardGameBinding;
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.utils.GlideUtils;

import java.util.ArrayList;
import java.util.List;

public final class GameAdapter extends RecyclerView.Adapter<GameAdapter.GameViewHolder> implements
        View.OnClickListener,
        View.OnLongClickListener
{
  private List<GameFile> mGameFiles;
  private Activity mActivity;

  /**
   * Initializes the adapter's observer, which watches for changes to the dataset. The adapter will
   * display no data until swapDataSet is called.
   */
  public GameAdapter(Activity activity)
  {
    mGameFiles = new ArrayList<>();
    mActivity = activity;
  }

  /**
   * Called by the LayoutManager when it is necessary to create a new view.
   *
   * @param parent   The RecyclerView (I think?) the created view will be thrown into.
   * @param viewType Not used here, but useful when more than one type of child will be used in the RecyclerView.
   * @return The created ViewHolder with references to all the child view's members.
   */
  @NonNull
  @Override
  public GameViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    CardGameBinding binding = CardGameBinding.inflate(LayoutInflater.from(parent.getContext()));

    binding.getRoot().setOnClickListener(this);
    binding.getRoot().setOnLongClickListener(this);

    // Use that view to create a ViewHolder.
    return new GameViewHolder(binding);
  }

  /**
   * Called by the LayoutManager when a new view is not necessary because we can recycle
   * an existing one (for example, if a view just scrolled onto the screen from the bottom, we
   * can use the view that just scrolled off the top instead of inflating a new one.)
   *
   * @param holder   A ViewHolder representing the view we're recycling.
   * @param position The position of the 'new' view in the dataset.
   */
  @Override
  public void onBindViewHolder(GameViewHolder holder, int position)
  {
    Context context = holder.itemView.getContext();
    GameFile gameFile = mGameFiles.get(position);
    GlideUtils.loadGameCover(holder, holder.binding.imageGameScreen, gameFile, mActivity);

    if (GameFileCacheManager.findSecondDisc(gameFile) != null)
    {
      holder.binding.textGameCaption
              .setText(context.getString(R.string.disc_number, gameFile.getDiscNumber() + 1));
    }
    else
    {
      holder.binding.textGameCaption.setText(gameFile.getCompany());
    }

    holder.gameFile = gameFile;

    Animation animateIn = AnimationUtils.loadAnimation(context, R.anim.anim_card_game_in);
    animateIn.setFillAfter(true);
    Animation animateOut = AnimationUtils.loadAnimation(context, R.anim.anim_card_game_out);
    animateOut.setFillAfter(true);
    holder.binding.getRoot().setOnFocusChangeListener((v, hasFocus) ->
            holder.binding.cardGameArt.startAnimation(hasFocus ? animateIn : animateOut));
  }

  public static class GameViewHolder extends RecyclerView.ViewHolder
  {
    public GameFile gameFile;
    public CardGameBinding binding;

    public GameViewHolder(@NonNull CardGameBinding binding)
    {
      super(binding.getRoot());
      binding.getRoot().setTag(this);
      this.binding = binding;
    }
  }

  /**
   * Called by the LayoutManager to find out how much data we have.
   *
   * @return Size of the dataset.
   */
  @Override
  public int getItemCount()
  {
    return mGameFiles.size();
  }

  /**
   * Tell Android whether or not each item in the dataset has a stable identifier.
   *
   * @param hasStableIds ignored.
   */
  @Override
  public void setHasStableIds(boolean hasStableIds)
  {
    super.setHasStableIds(false);
  }

  /**
   * When a load is finished, call this to replace the existing data
   * with the newly-loaded data.
   */
  public void swapDataSet(List<GameFile> gameFiles)
  {
    mGameFiles = gameFiles;
    notifyDataSetChanged();
  }

  /**
   * Re-fetches game metadata from the game file cache.
   */
  public void refetchMetadata()
  {
    notifyItemRangeChanged(0, getItemCount());
  }

  /**
   * Launches the game that was clicked on.
   *
   * @param view The card representing the game the user wants to play.
   */
  @Override
  public void onClick(View view)
  {
    GameViewHolder holder = (GameViewHolder) view.getTag();

    String[] paths = GameFileCacheManager.findSecondDiscAndGetPaths(holder.gameFile);
    EmulationActivity.launch((FragmentActivity) view.getContext(), paths, false);
  }

  /**
   * Launches the details activity for this Game, using an ID stored in the
   * details button's Tag.
   *
   * @param view The Card button that was long-clicked.
   */
  @Override
  public boolean onLongClick(View view)
  {
    GameViewHolder holder = (GameViewHolder) view.getTag();

    GamePropertiesDialog fragment = GamePropertiesDialog.newInstance(holder.gameFile);
    ((FragmentActivity) view.getContext()).getSupportFragmentManager().beginTransaction()
            .add(fragment, GamePropertiesDialog.TAG).commit();

    return true;
  }
}
