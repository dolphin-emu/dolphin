package org.dolphinemu.dolphinemu.adapters;

import android.content.Context;
import android.support.v4.app.FragmentActivity;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.dialogs.GameDetailsDialog;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.viewholders.GameViewHolder;

import java.util.ArrayList;
import java.util.List;

public final class GameAdapter extends RecyclerView.Adapter<GameViewHolder> implements
  View.OnClickListener,
  View.OnLongClickListener
{
  private int mResourceId;
  private List<GameFile> mGameFiles;

  /**
   * Initializes the adapter's observer, which watches for changes to the dataset. The adapter will
   * display no data until swapDataSet is called.
   */
  public GameAdapter()
  {
    mGameFiles = new ArrayList<>();
  }

  /**
   * Called by the LayoutManager when it is necessary to create a new view.
   *
   * @param parent   The RecyclerView (I think?) the created view will be thrown into.
   * @param viewType Not used here, but useful when more than one type of child will be used in the RecyclerView.
   * @return The created ViewHolder with references to all the child view's members.
   */
  @Override
  public GameViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    // Create a new view.
    View gameCard = LayoutInflater.from(parent.getContext())
      .inflate(viewType, parent, false);

    gameCard.setOnClickListener(this);
    gameCard.setOnLongClickListener(this);

    // Use that view to create a ViewHolder.
    return new GameViewHolder(gameCard);
  }

  @Override
  public int getItemViewType(int position)
  {
    return mResourceId;
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
    GameFile gameFile = mGameFiles.get(position);
    gameFile.loadGameBanner(holder.imageScreenshot);

    holder.textGameTitle.setText(gameFile.getTitle());
    holder.textCompany.setText(gameFile.getCompany());

    final int[] platforms = {
      R.string.game_platform_ngc,
      R.string.game_platform_wii,
      R.string.game_platform_ware,
      R.string.game_platform_n64,
      R.string.game_platform_nes,
      R.string.game_platform_sms,
      R.string.game_platform_smd,
      R.string.game_platform_c64,
      R.string.game_platform_snes,
    };
    final Context context = holder.textPlatform.getContext();
    final String[] countryNames = context.getResources().getStringArray(R.array.countryNames);
    int platform = gameFile.getPlatform();
    int country = gameFile.getCountry();
    int discNumber = gameFile.getDiscNumber() + 1;
    if (platform == 2)
    {
      // WiiWAD, Virtual Console
      String gameId = gameFile.getGameId();
      switch (gameId.charAt(0))
      {
        case 'N':
          // N64
          platform = 3;
          break;
        case 'F':
          // NES
          platform = 4;
          break;
        case 'L':
          // SMS
          platform = 5;
          break;
        case 'M':
          // SMD
          platform = 6;
          break;
        case 'C':
          // C64
          platform = 7;
          break;
        case 'J':
          // SNES
          platform = 8;
          break;
      }
    }
    String discInfo = discNumber > 1 ? "DISC-" + discNumber : "";
    if (platform < 0 || platform >= platforms.length)
      platform = 2;
    if (country < 0 || country >= countryNames.length)
      country = countryNames.length - 1;
    holder.textPlatform.setText(context.getString(platforms[platform], countryNames[country], discInfo));
    holder.gameFile = gameFile;
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

  public void setResourceId(int resId)
  {
    mResourceId = resId;
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
    EmulationActivity.launch(view.getContext(), holder.gameFile, null);
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
    FragmentActivity activity = (FragmentActivity) view.getContext();
    GameViewHolder holder = (GameViewHolder) view.getTag();
    GameDetailsDialog.newInstance(holder.gameFile.getPath()).show(
      activity.getSupportFragmentManager(), "GameDetailsDialog");
    return true;
  }
}
