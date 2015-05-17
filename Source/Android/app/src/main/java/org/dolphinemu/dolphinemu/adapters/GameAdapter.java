package org.dolphinemu.dolphinemu.adapters;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.dialogs.GameDetailsDialog;
import org.dolphinemu.dolphinemu.emulation.EmulationActivity;
import org.dolphinemu.dolphinemu.model.Game;
import org.dolphinemu.dolphinemu.viewholders.GameViewHolder;

import java.util.ArrayList;

public class GameAdapter extends RecyclerView.Adapter<GameViewHolder> implements
		View.OnClickListener,
		View.OnLongClickListener
{
	private ArrayList<Game> mGameList;

	/**
	 * Mostly just initializes the dataset to be displayed.
	 *
	 * @param gameList
	 */
	public GameAdapter(ArrayList<Game> gameList)
	{
		mGameList = gameList;
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
		GameViewHolder holder = new GameViewHolder(gameCard);
		return holder;
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
		// Get a reference to the item from the dataset; we'll use this to fill in the view contents.
		final Game game = mGameList.get(position);

		// Fill in the view contents.
		Picasso.with(holder.imageScreenshot.getContext())
				.load(game.getScreenPath())
				.fit()
				.centerCrop()
				.error(R.drawable.no_banner)
				.into(holder.imageScreenshot);

		holder.textGameTitle.setText(game.getTitle());
		if (game.getCompany() != null)
		{
			holder.textCompany.setText(game.getCompany());
		}

		holder.path = game.getPath();
		holder.screenshotPath = game.getScreenPath();
		holder.game = game;
	}

	/**
	 * Called by the LayoutManager to find out how much data we have.
	 *
	 * @return Size of the dataset.
	 */
	@Override
	public int getItemCount()
	{
		return mGameList.size();
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

		// Start the emulation activity and send the path of the clicked ISO to it.
		Intent intent = new Intent(view.getContext(), EmulationActivity.class);

		intent.putExtra("SelectedGame", holder.path);

		view.getContext().startActivity(intent);
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

		// Get the ID of the game we want to look at.
		// TODO This should be all we need to pass in, eventually.
		// String gameId = (String) holder.gameId;

		Activity activity = (Activity) view.getContext();
		GameDetailsDialog.newInstance(holder.game).show(activity.getFragmentManager(), "game_details");

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
		public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state)
		{
			outRect.left = space;
			outRect.right = space;
			outRect.bottom = space;
			outRect.top = space;
		}
	}

	public void setGameList(ArrayList<Game> gameList)
	{
		mGameList = gameList;
		notifyDataSetChanged();
	}
}
