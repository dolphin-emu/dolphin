package org.dolphinemu.dolphinemu.adapters;

import android.graphics.Bitmap;
import android.support.v17.leanback.widget.ImageCardView;
import android.support.v17.leanback.widget.Presenter;
import android.view.ViewGroup;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.viewholders.TvGameViewHolder;

/**
 * The Leanback library / docs call this a Presenter, but it works very
 * similarly to a RecyclerView.ViewHolder.
 */
public final class GamePresenter extends Presenter
{
	private int mPlatform;

	public GamePresenter(int platform)
	{
		mPlatform = platform;
	}

	public ViewHolder onCreateViewHolder(ViewGroup parent)
	{
		// Create a new view.
		ImageCardView gameCard = new ImageCardView(parent.getContext())
		{
			@Override
			public void setSelected(boolean selected)
			{
				setCardBackground(this, selected);
				super.setSelected(selected);
			}
		};

		gameCard.setMainImageAdjustViewBounds(true);
		gameCard.setMainImageDimensions(480, 320);
		gameCard.setMainImageScaleType(ImageView.ScaleType.CENTER_CROP);

		gameCard.setFocusable(true);
		gameCard.setFocusableInTouchMode(true);

		setCardBackground(gameCard, false);

		// Use that view to create a ViewHolder.
		return new TvGameViewHolder(gameCard);
	}

	public void onBindViewHolder(ViewHolder viewHolder, Object item)
	{
		TvGameViewHolder holder = (TvGameViewHolder) viewHolder;
		Game game = (Game) item;

		String screenPath = game.getScreenshotPath();

		// Fill in the view contents.
		Picasso.with(holder.imageScreenshot.getContext())
				.load(screenPath)
				.fit()
				.centerCrop()
				.noFade()
				.noPlaceholder()
				.config(Bitmap.Config.RGB_565)
				.error(R.drawable.no_banner)
				.into(holder.imageScreenshot);

		holder.cardParent.setTitleText(game.getTitle());
		holder.cardParent.setContentText(game.getCompany());

		// TODO These shouldn't be necessary once the move to a DB-based model is complete.
		holder.gameId = game.getGameId();
		holder.path = game.getPath();
		holder.title = game.getTitle();
		holder.description = game.getDescription();
		holder.country = game.getCountry();
		holder.company = game.getCompany();
		holder.screenshotPath = game.getScreenshotPath();
	}

	public void onUnbindViewHolder(ViewHolder viewHolder)
	{
		// no op
	}

	public void setCardBackground(ImageCardView view, boolean selected)
	{
		int backgroundColor;

		if (selected)
		{
			switch (mPlatform)
			{
				case Game.PLATFORM_GC:
					backgroundColor = R.color.dolphin_accent_gamecube;
					break;

				case Game.PLATFORM_WII:
					backgroundColor = R.color.dolphin_accent_wii;
					break;

				case Game.PLATFORM_WII_WARE:
					backgroundColor = R.color.dolphin_accent_wiiware;
					break;

				default:
					backgroundColor = android.R.color.holo_red_dark;
					break;
			}
		}
		else
		{
			backgroundColor = R.color.tv_card_unselected;
		}

		view.setInfoAreaBackgroundColor(view.getResources().getColor(backgroundColor));
	}
}
