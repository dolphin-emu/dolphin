package org.dolphinemu.dolphinemu.adapters;

import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import androidx.recyclerview.widget.RecyclerView;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.utils.PicassoUtils;
import org.dolphinemu.dolphinemu.viewholders.GameViewHolder;

import java.util.ArrayList;
import java.util.List;

public final class GameAdapter extends RecyclerView.Adapter<GameViewHolder> implements
        View.OnClickListener,
        View.OnLongClickListener
{
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
            .inflate(R.layout.card_game, parent, false);

    gameCard.setOnClickListener(this);
    gameCard.setOnLongClickListener(this);

    // Use that view to create a ViewHolder.
    return new GameViewHolder(gameCard);
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
    PicassoUtils.loadGameCover(holder.imageScreenshot, gameFile);

    holder.textGameTitle.setText(gameFile.getTitle());
    holder.textCompany.setText(gameFile.getCompany());

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

  /**
   * Launches the game that was clicked on.
   *
   * @param view The card representing the game the user wants to play.
   */
  @Override
  public void onClick(View view)
  {
    GameViewHolder holder = (GameViewHolder) view.getTag();

    EmulationActivity.launch((FragmentActivity) view.getContext(), holder.gameFile);
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
    String gameId = holder.gameFile.getGameId();

    if (gameId.isEmpty())
    {
      AlertDialog.Builder builder = new AlertDialog.Builder(activity);
      builder.setTitle("Game Settings");
      builder.setMessage("Files without game IDs don't support game-specific settings.");

      builder.show();
      return true;
    }

    GamePropertiesDialog fragment =
            GamePropertiesDialog
                    .newInstance(holder.gameFile.getPath(), gameId, holder.gameFile.getPlatform());
    ((FragmentActivity) view.getContext()).getSupportFragmentManager().beginTransaction()
            .add(fragment, GamePropertiesDialog.TAG).commit();
    return true;
  }

  public static class SpacesItemDecoration extends RecyclerView.ItemDecoration
  {
    private int space;

    public SpacesItemDecoration(int space)
    {
      this.space = space;
    }

    @Override
    public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state)
    {
      outRect.left = space;
      outRect.right = space;
      outRect.bottom = space;
      outRect.top = space;
    }
  }
}
