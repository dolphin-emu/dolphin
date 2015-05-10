package org.dolphinemu.dolphinemu.adapters;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.FileListItem;
import org.dolphinemu.dolphinemu.viewholders.FileViewHolder;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;

public class FileAdapter extends RecyclerView.Adapter<FileViewHolder> implements View.OnClickListener
{
	private ArrayList<FileListItem> mFileList;

	private String mPath;

	private FileClickListener mListener;

	/**
	 * Initializes the dataset to be displayed, and associates the Adapter with the
	 * Activity as an event listener.
	 *
	 * @param gameList
	 */
	public FileAdapter(String path, FileClickListener listener)
	{
		mFileList = generateFileList(new File(path));
		mListener = listener;
	}

	/**
	 * Called by the LayoutManager when it is necessary to create a new view.
	 *
	 * @param parent   The RecyclerView (I think?) the created view will be thrown into.
	 * @param viewType Not used here, but useful when more than one type of child will be used in the RecyclerView.
	 * @return The created ViewHolder with references to all the child view's members.
	 */
	@Override
	public FileViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
	{
		// Create a new view.
		View listItem = LayoutInflater.from(parent.getContext())
				.inflate(R.layout.list_item_file, parent, false);

		listItem.setOnClickListener(this);

		// Use that view to create a ViewHolder.
		return new FileViewHolder(listItem);
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
	public void onBindViewHolder(FileViewHolder holder, int position)
	{
		// Get a reference to the item from the dataset; we'll use this to fill in the view contents.
		final FileListItem file = mFileList.get(position);

		// Fill in the view contents.
		switch (file.getType())
		{
			case FileListItem.TYPE_FOLDER:
				holder.imageType.setImageResource(R.drawable.ic_folder);
				break;

			case FileListItem.TYPE_GC:
				holder.imageType.setImageResource(R.drawable.ic_gamecube);
				break;

			case FileListItem.TYPE_WII:
				holder.imageType.setImageResource(R.drawable.ic_wii);
				break;

			case FileListItem.TYPE_OTHER:
				holder.imageType.setImageResource(android.R.color.transparent);
				break;
		}

		holder.textFileName.setText(file.getFilename());
		holder.itemView.setTag(file.getPath());
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

	/**
	 * When a file is clicked, determine if it is a directory; if it is, show that new directory's
	 * contents. If it is not, end the activity successfully.
	 *
	 * @param view
	 */
	@Override
	public void onClick(final View view)
	{
		String path = (String) view.getTag();

		File clickedFile = new File(path);

		if (clickedFile.isDirectory())
		{
			final ArrayList<FileListItem> fileList = generateFileList(clickedFile);

			if (fileList.isEmpty())
			{
				Toast.makeText(view.getContext(), R.string.add_directory_empty_folder, Toast.LENGTH_SHORT).show();
			} else
			{
				// Delay the loading of the new directory to give a little bit of time for UI feedback
				// to happen. Hacky, but good enough for now; this is necessary because we're modifying
				// the RecyclerView's contents, rather than constructing a new one.
				view.getHandler().postDelayed(new Runnable()
				{
					@Override
					public void run()
					{
						mFileList = fileList;
						notifyDataSetChanged();
					}
				}, 200);
			}
		} else
		{
			// Pass the activity the path of the parent directory of the clicked file.
			mListener.finishSuccessfully();
		}
	}

	/**
	 * For a given directory, return a list of Files it contains.
	 *
	 * @param directory
	 * @return
	 */
	private ArrayList<FileListItem> generateFileList(File directory)
	{
		File[] children = directory.listFiles();
		ArrayList<FileListItem> fileList = new ArrayList<FileListItem>(children.length);

		for (File child : children)
		{
			if (!child.isHidden())
			{
				FileListItem item = new FileListItem(child);
				fileList.add(item);
			}
		}

		mPath = directory.getAbsolutePath();

		Collections.sort(fileList);
		return fileList;
	}

	public String getPath()
	{
		return mPath;
	}

	/**
	 * Mostly just allows the activity's menu option to kick us up a level in the directory
	 * structure.
	 *
	 * @param path
	 */
	public void setPath(String path)
	{
		mPath = path;
		File parentDirectory = new File(path);

		mFileList = generateFileList(parentDirectory);
		notifyDataSetChanged();
	}

	/**
	 * Callback for when the user wants to add the visible directory to the library.
	 */
	public interface FileClickListener
	{
		void finishSuccessfully();
	}
}
