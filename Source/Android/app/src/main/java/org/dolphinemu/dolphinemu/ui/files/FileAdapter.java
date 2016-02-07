package org.dolphinemu.dolphinemu.ui.files;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.FileListItem;

import java.util.List;

public final class FileAdapter extends RecyclerView.Adapter<FileViewHolder>
{
	private List<FileListItem> mFileList;

	private FileListView mView;

	public FileAdapter(List<FileListItem> fileList, FileListView view)
	{
		mFileList = fileList;
		mView = view;
	}

	@Override
	public FileViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
	{
		// Create a new view.
		View listItem = LayoutInflater.from(parent.getContext())
				.inflate(R.layout.list_item_file, parent, false);

		// Use that view to create a ViewHolder.
		return new FileViewHolder(listItem, this);
	}

	@Override
	public void onBindViewHolder(FileViewHolder holder, int position)
	{
		// Get a reference to the item from the dataset; we'll use this to fill in the view contents.
		final FileListItem file = mFileList.get(position);

		holder.bind(file);
	}

	/**
	 * Called by the LayoutManager to find out how much data we have.
	 *
	 * @return Size of the dataset.
	 */
	@Override
	public int getItemCount()
	{
		return mFileList.size();
	}

	public void onItemClick(String path)
	{
		mView.onItemClick(path);
	}
}
