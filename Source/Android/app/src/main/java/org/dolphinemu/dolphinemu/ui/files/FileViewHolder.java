package org.dolphinemu.dolphinemu.ui.files;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.FileListItem;

/**
 * A simple class that stores references to views so that the FileAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
public class FileViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
{
	private FileAdapter mAdapter;
	private FileListItem mItem;


	public TextView mTextFileName;
	public ImageView mImageType;

	public FileViewHolder(View itemView, FileAdapter adapter)
	{
		super(itemView);

		mAdapter = adapter;

		itemView.setOnClickListener(this);

		findViews(itemView);
	}

	public void bind(FileListItem item)
	{
		mItem = item;

		// Fill in the view contents.
		switch (item.getType())
		{
			case FileListItem.TYPE_FOLDER:
				mImageType.setImageResource(R.drawable.ic_folder);
				break;

			case FileListItem.TYPE_GC:
				mImageType.setImageResource(R.drawable.ic_gamecube);
				break;

			case FileListItem.TYPE_WII:
				mImageType.setImageResource(R.drawable.ic_wii);
				break;

			case FileListItem.TYPE_OTHER:
				mImageType.setImageResource(android.R.color.transparent);
				break;
		}

		mTextFileName.setText(item.getFilename());
	}

	public void findViews(View itemView)
	{
		mTextFileName = (TextView) itemView.findViewById(R.id.text_file_name);
		mImageType = (ImageView) itemView.findViewById(R.id.image_type);
	}

	@Override
	public void onClick(View v)
	{
		String path = mItem.getPath();

		mAdapter.onItemClick(path);
	}
}
