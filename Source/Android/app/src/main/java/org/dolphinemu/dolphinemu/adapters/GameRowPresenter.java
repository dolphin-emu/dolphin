package org.dolphinemu.dolphinemu.adapters;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.app.AlertDialog;
import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;
import androidx.fragment.app.FragmentActivity;
import androidx.core.content.ContextCompat;

import android.view.ViewGroup;
import android.widget.ImageView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.PicassoUtils;
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder;

/**
 * The Leanback library / docs call this a Presenter, but it works very
 * similarly to a RecyclerView.Adapter.
 */
public final class GameRowPresenter extends Presenter
{
  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent)
  {
    // Create a new view.
    ImageCardView gameCard = new ImageCardView(parent.getContext());

    gameCard.setMainImageAdjustViewBounds(true);
    gameCard.setMainImageDimensions(240, 336);
    gameCard.setMainImageScaleType(ImageView.ScaleType.CENTER_CROP);

    gameCard.setFocusable(true);
    gameCard.setFocusableInTouchMode(true);

    // Use that view to create a ViewHolder.
    return new TvGameViewHolder(gameCard);
  }

  @Override
  public void onBindViewHolder(ViewHolder viewHolder, Object item)
  {
    TvGameViewHolder holder = (TvGameViewHolder) viewHolder;
    GameFile gameFile = (GameFile) item;

    holder.imageScreenshot.setImageDrawable(null);
    PicassoUtils.loadGameCover(holder.imageScreenshot, gameFile);

    holder.cardParent.setTitleText(gameFile.getTitle());
    holder.cardParent.setContentText(gameFile.getCompany());

    holder.gameFile = gameFile;

    // Set the platform-dependent background color of the card
    int backgroundId;
    switch (Platform.fromNativeInt(gameFile.getPlatform()))
    {
      case GAMECUBE:
        backgroundId = R.drawable.tv_card_background_gamecube;
        break;
      case WII:
        backgroundId = R.drawable.tv_card_background_wii;
        break;
      case WIIWARE:
        backgroundId = R.drawable.tv_card_background_wiiware;
        break;
      default:
        throw new AssertionError("Not reachable.");
    }
    Context context = holder.cardParent.getContext();
    Drawable background = ContextCompat.getDrawable(context, backgroundId);
    holder.cardParent.setInfoAreaBackground(background);
    holder.cardParent.setOnLongClickListener((view) ->
    {
      FragmentActivity activity = (FragmentActivity) view.getContext();
      String gameId = gameFile.getGameId();

      if (gameId.isEmpty())
      {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle("Game Settings");
        builder.setMessage("Files without game IDs don't support game-specific settings.");

        builder.show();
        return true;
      }

      GamePropertiesDialog fragment =
              GamePropertiesDialog.newInstance(holder.gameFile.getPath(), gameId,
                      holder.gameFile.getPlatform());
      ((FragmentActivity) view.getContext()).getSupportFragmentManager().beginTransaction()
              .add(fragment, GamePropertiesDialog.TAG).commit();

      return true;
    });
  }

  @Override
  public void onUnbindViewHolder(ViewHolder viewHolder)
  {
    // no op
  }
}
