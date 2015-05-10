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

	// Used to handle onClick(). Set this in onBindViewHolder().
	public String path;
	public String screenshotPath;
	public Game game;

	public GameViewHolder(View itemView)
	{
		super(itemView);

		itemView.setTag(this);

		imageScreenshot = (ImageView) itemView.findViewById(R.id.image_game_screen);
		textGameTitle = (TextView) itemView.findViewById(R.id.text_game_title);
		textDescription = (TextView) itemView.findViewById(R.id.text_game_description);
	}
}
