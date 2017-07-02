package org.dolphinemu.dolphinemu.adapters;

import android.app.Activity;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.dialogs.GameDetailsDialog;
import org.dolphinemu.dolphinemu.model.GameDatabase;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.PicassoUtils;
import org.dolphinemu.dolphinemu.viewholders.GameViewHolder;

/**
 * This adapter, unlike {@link FileAdapter} which is backed by an ArrayList, gets its
 * information from a database Cursor. This fact, paired with the usage of ContentProviders
 * and Loaders, allows for efficient display of a limited view into a (possibly) large dataset.
 */
public final class GameAdapter extends RecyclerView.Adapter<GameViewHolder> implements
		View.OnClickListener,
		View.OnLongClickListener
{
	private Cursor mCursor;
	private GameDataSetObserver mObserver;

	private boolean mDatasetValid;

	/**
	 * Initializes the adapter's observer, which watches for changes to the dataset. The adapter will
	 * display no data until a Cursor is supplied by a CursorLoader.
	 */
	public GameAdapter()
	{
		mDatasetValid = false;
		mObserver = new GameDataSetObserver();
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
		if (mDatasetValid)
		{
			if (mCursor.moveToPosition(position))
			{
				String screenPath = mCursor.getString(GameDatabase.GAME_COLUMN_SCREENSHOT_PATH);
				PicassoUtils.loadGameBanner(holder.imageScreenshot, screenPath, mCursor.getString(GameDatabase.GAME_COLUMN_PATH));

				holder.textGameTitle.setText(mCursor.getString(GameDatabase.GAME_COLUMN_TITLE));
				holder.textCompany.setText(mCursor.getString(GameDatabase.GAME_COLUMN_COMPANY));

				// TODO These shouldn't be necessary once the move to a DB-based model is complete.
				holder.gameId = mCursor.getString(GameDatabase.GAME_COLUMN_GAME_ID);
				holder.path = mCursor.getString(GameDatabase.GAME_COLUMN_PATH);
				holder.title = mCursor.getString(GameDatabase.GAME_COLUMN_TITLE);
				holder.description = mCursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION);
				holder.country = mCursor.getInt(GameDatabase.GAME_COLUMN_COUNTRY);
				holder.company = mCursor.getString(GameDatabase.GAME_COLUMN_COMPANY);
				holder.screenshotPath = mCursor.getString(GameDatabase.GAME_COLUMN_SCREENSHOT_PATH);
			}
			else
			{
				Log.error("[GameAdapter] Can't bind view; Cursor is not valid.");
			}
		}
		else
		{
			Log.error("[GameAdapter] Can't bind view; dataset is not valid.");
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
		if (mDatasetValid && mCursor != null)
		{
			return mCursor.getCount();
		}
		Log.error("[GameAdapter] Dataset is not valid.");
		return 0;
	}

	/**
	 * Return the contents of the _id column for a given row.
	 *
	 * @param position The row for which Android wants an ID.
	 * @return A valid ID from the database, or 0 if not available.
	 */
	@Override
	public long getItemId(int position)
	{
		if (mDatasetValid && mCursor != null)
		{
			if (mCursor.moveToPosition(position))
			{
				return mCursor.getLong(GameDatabase.COLUMN_DB_ID);
			}
		}

		Log.error("[GameAdapter] Dataset is not valid.");
		return 0;
	}

	/**
	 * Tell Android whether or not each item in the dataset has a stable identifier.
	 * Which it does, because it's a database, so always tell Android 'true'.
	 *
	 * @param hasStableIds ignored.
	 */
	@Override
	public void setHasStableIds(boolean hasStableIds)
	{
		super.setHasStableIds(true);
	}

	/**
	 * When a load is finished, call this to replace the existing data with the newly-loaded
	 * data.
	 *
	 * @param cursor The newly-loaded Cursor.
	 */
	public void swapCursor(Cursor cursor)
	{
		// Sanity check.
		if (cursor == mCursor)
		{
			return;
		}

		// Before getting rid of the old cursor, disassociate it from the Observer.
		final Cursor oldCursor = mCursor;
		if (oldCursor != null && mObserver != null)
		{
			oldCursor.unregisterDataSetObserver(mObserver);
		}

		mCursor = cursor;
		if (mCursor != null)
		{
			// Attempt to associate the new Cursor with the Observer.
			if (mObserver != null)
			{
				mCursor.registerDataSetObserver(mObserver);
			}

			mDatasetValid = true;
		}
		else
		{
			mDatasetValid = false;
		}

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

		EmulationActivity.launch((Activity) view.getContext(),
				holder.path,
				holder.title,
				holder.screenshotPath,
				holder.getAdapterPosition(),
				holder.imageScreenshot);
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
		GameDetailsDialog.newInstance(holder.title,
				holder.description,
				holder.country,
				holder.company,
				holder.path,
				holder.screenshotPath).show(activity.getFragmentManager(), "game_details");

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

	private final class GameDataSetObserver extends DataSetObserver
	{
		@Override
		public void onChanged()
		{
			super.onChanged();

			mDatasetValid = true;
			notifyDataSetChanged();
		}

		@Override
		public void onInvalidated()
		{
			super.onInvalidated();

			mDatasetValid = false;
			notifyDataSetChanged();
		}
	}
}
