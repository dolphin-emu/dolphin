package org.dolphinemu.dolphinemu.model;


import org.dolphinemu.dolphinemu.NativeLibrary;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class FileListItem implements Comparable<FileListItem>
{
	public static final int TYPE_GC = 0;
	public static final int TYPE_WII = 1;
	public static final int TYPE_WII_WARE = 2;
	public static final int TYPE_OTHER = 4;
	public static final int TYPE_FOLDER = 5;

	private int mType;
	private String mFilename;
	private String mPath;

	public FileListItem(File file)
	{
		mPath = file.getAbsolutePath();
		mFilename = file.getName();

		if (file.isDirectory())
		{
			mType = TYPE_FOLDER;
		}
		else
		{
			int extensionStart = mPath.lastIndexOf('.');
			if (extensionStart < 1)
			{
				// Ignore hidden files & files without extensions.
				mType = TYPE_OTHER;
			}
			else
			{
				String fileExtension = mPath.substring(extensionStart);

				// The extensions we care about.
				Set<String> allowedExtensions = new HashSet<String>(Arrays.asList(".dff", ".dol", ".elf", ".gcm", ".gcz", ".iso", ".wad", ".wbfs"));

				// Check that the file has an extension we care about before trying to read out of it.
				if (allowedExtensions.contains(fileExtension))
				{
					mType = NativeLibrary.GetPlatform(mPath);
				}
				else
				{
					mType = TYPE_OTHER;
				}
			}
		}
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
