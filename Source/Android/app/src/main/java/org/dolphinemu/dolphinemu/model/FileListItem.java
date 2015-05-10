package org.dolphinemu.dolphinemu.model;


import org.dolphinemu.dolphinemu.NativeLibrary;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class FileListItem implements Comparable<FileListItem>
{
	public static final int TYPE_FOLDER = 0;
	public static final int TYPE_GC = 1;
	public static final int TYPE_WII = 2;
	public static final int TYPE_OTHER = 3;

	private int mType;
	private String mFilename;
	private String mPath;

	public FileListItem(File file)
	{
		mPath = file.getAbsolutePath();

		if (file.isDirectory())
		{
			mType = TYPE_FOLDER;
		}
		else
		{
			String fileExtension = mPath.substring(mPath.lastIndexOf('.'));

			// Extensions to filter by.
			Set<String> allowedExtensions = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

			// Check that the file has an appropriate extension before trying to read out of it.
			if (allowedExtensions.contains(fileExtension))
			{
				mType = NativeLibrary.IsWiiTitle(mPath) ? TYPE_WII : TYPE_GC;
			}
			else
			{
				mType = TYPE_OTHER;
			}
		}

		mFilename = file.getName();
	}

	public int getType()
	{
		return mType;
	}

	public String getFilename()
	{
		return mFilename;
	}

	public String getPath()
	{
		return mPath;
	}

	@Override
	public int compareTo(FileListItem theOther)
	{
		if (theOther.getType() == getType())
		{
			return getFilename().toLowerCase().compareTo(theOther.getFilename().toLowerCase());
		}
		else
		{
			if (getType() > theOther.getType())
			{
				return 1;
			}
			else
			{
				return -1;
			}
		}
	}
}
