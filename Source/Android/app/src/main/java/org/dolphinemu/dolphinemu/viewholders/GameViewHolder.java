package org.dolphinemu.dolphinemu.viewholders;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.Game;


public class GameViewHolder extends RecyclerView.ViewHolder
{
	public ImageView imageScreenshot;
	public TextView textGameTitle;
	public TextView textCompany;

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
		textCompany = (TextView) itemView.findViewById(R.id.text_company);
	}
}
