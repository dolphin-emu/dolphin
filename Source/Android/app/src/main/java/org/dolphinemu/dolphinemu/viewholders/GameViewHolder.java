package org.dolphinemu.dolphinemu.viewholders;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public class GameViewHolder extends RecyclerView.ViewHolder
{
  public ImageView imageScreenshot;
  public TextView textGameTitle;
  public TextView textCompany;
  public TextView textPlatform;

  public GameFile gameFile;

  public GameViewHolder(View itemView)
  {
    super(itemView);

    itemView.setTag(this);

    imageScreenshot = itemView.findViewById(R.id.image_game_screen);
    textGameTitle = itemView.findViewById(R.id.text_game_title);
    textCompany = itemView.findViewById(R.id.text_company);
    textPlatform = itemView.findViewById(R.id.text_platform);
  }
}
