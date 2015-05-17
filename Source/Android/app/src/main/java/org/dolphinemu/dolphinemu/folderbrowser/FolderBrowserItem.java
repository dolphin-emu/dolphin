/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.folderbrowser;

import java.io.File;

/**
 * Represents an item in the {@link FolderBrowser} list.
 */
public final class FolderBrowserItem implements Comparable<FolderBrowserItem>
{
	private final String name;
	private final String subtitle;
	private final String path;
	private final File underlyingFile;

	/**
	 * Constructor
	 * 
	 * @param name     The name of the file/folder represented by this item.
	 * @param subtitle The subtitle of this FolderBrowserItem.
	 * @param path     The path of the file/folder represented by this item.
	 */
	public FolderBrowserItem(String name, String subtitle, String path)
	{
		this.name = name;
		this.subtitle = subtitle;
		this.path = path;
		this.underlyingFile = new File(path);
	}

	/**
	 * Constructor. Initializes a FolderBrowserItem with an empty subtitle.
	 * 
	 * @param name The name of the file/folder represented by this item.
	 * @param path The path of the file/folder represented by this item.
	 */
	public FolderBrowserItem(String name, String path)
	{
		this.name = name;
		this.subtitle = "";
		this.path = path;
		this.underlyingFile = new File(path);
	}

	/**
	 * Gets the name of the file/folder represented by this FolderBrowserItem.
	 * 
	 * @return the name of the file/folder represented by this FolderBrowserItem.
	 */
	public String getName()
	{
		return name;
	}

	/**
	 * Gets the subtitle text of this FolderBrowserItem.
	 * 
	 * @return the subtitle text of this FolderBrowserItem.
	 */
	public String getSubtitle()
	{
		return subtitle;
	}

	/**
	 * Gets the path of the file/folder represented by this FolderBrowserItem.
	 * 
	 * @return the path of the file/folder represented by this FolderBrowserItem.
	 */
	public String getPath()
	{
		return path;
	}

	/**
	 * Gets the {@link File} representation of the underlying file/folder
	 * represented by this FolderBrowserItem.
	 * 
	 * @return the {@link File} representation of the underlying file/folder
	 * represented by this FolderBrowserItem.
	 */
	public File getUnderlyingFile()
	{
		return underlyingFile;
	}

	/**
	 * Gets whether or not this FolderBrowserItem represents a directory.
	 * 
	 * @return true if this FolderBrowserItem represents a directory, false otherwise.
	 */
	public boolean isDirectory()
	{
		return underlyingFile.isDirectory();
	}

	@Override
	public int compareTo(FolderBrowserItem other)
	{
		if(name != null)
			return name.toLowerCase().compareTo(other.getName().toLowerCase()); 
		else 
			throw new NullPointerException("The name of this FolderBrowserItem is null");
	}
}
