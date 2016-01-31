package org.dolphinemu.dolphinemu.ui.files;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

/**
 * A simple class that stores references to views so that the FileAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public class FileViewHolder extends RecyclerView.ViewHolder
{
	public View itemView;

	public TextView textFileName;
	public ImageView imageType;

	public FileViewHolder(View itemView)
	{
		super(itemView);

		this.itemView = itemView;

		textFileName = (TextView) itemView.findViewById(R.id.text_file_name);
		imageType = (ImageView) itemView.findViewById(R.id.image_type);
	}
}
