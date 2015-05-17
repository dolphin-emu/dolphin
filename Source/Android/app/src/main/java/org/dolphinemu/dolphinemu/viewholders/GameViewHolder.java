package org.dolphinemu.dolphinemu.viewholders;

import android.app.Activity;
import android.content.Intent;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.dialogs.GameDetailsDialog;
import org.dolphinemu.dolphinemu.emulation.EmulationActivity;
import org.dolphinemu.dolphinemu.model.Game;


public class GameViewHolder extends RecyclerView.ViewHolder
{
	public ImageView imageScreenshot;
	public TextView textGameTitle;
	public TextView textDescription;
	public ImageButton buttonDetails;

	// Used to handle onClick(). Set this in onBindViewHolder().
	public String path;
	public String screenshotPath;
	public Game game;

	public GameViewHolder(View itemView)
	{
		super(itemView);

		itemView.setOnClickListener(mCardClickListener);

		imageScreenshot = (ImageView) itemView.findViewById(R.id.image_game_screen);
		textGameTitle = (TextView) itemView.findViewById(R.id.text_game_title);
		textDescription = (TextView) itemView.findViewById(R.id.text_game_description);
		buttonDetails = (ImageButton) itemView.findViewById(R.id.button_details);

		buttonDetails.setOnClickListener(mDetailsButtonListener);
	}

	private View.OnClickListener mCardClickListener = new View.OnClickListener()
	{
		/**
		 * Launches the game that was clicked on.
		 *
		 * @param view The card representing the game the user wants to play.
		 */
		@Override
		public void onClick(View view)
		{
			// Start the emulation activity and send the path of the clicked ROM to it.
			Intent intent = new Intent(view.getContext(), EmulationActivity.class);

			intent.putExtra("SelectedGame", path);

			view.getContext().startActivity(intent);
		}
	};

	private View.OnClickListener mDetailsButtonListener = new View.OnClickListener()
	{

		/**
		 * Launches the details activity for this Game, using an ID stored in the
		 * details button's Tag.
		 *
		 * @param view The Details button that was clicked on.
		 */
		@Override
		public void onClick(View view)
		{
			// Get the ID of the game we want to look at.
			// TODO This should be all we need to pass in, eventually.
			// String gameId = (String) view.getTag();

			Activity activity = (Activity) view.getContext();
			GameDetailsDialog.newInstance(game).show(activity.getFragmentManager(), "game_details");
		}
	};

}
