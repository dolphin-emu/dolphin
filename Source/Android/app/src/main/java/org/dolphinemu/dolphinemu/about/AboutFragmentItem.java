/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

/**
 * Represents an item within an info fragment in the About menu.
 */
final class AboutFragmentItem
{
	private final String title;
	private final String subtitle;

	/**
	 * Constructor
	 * 
	 * @param title    The title of this item.
	 * @param subtitle The subtitle for this item.
	 */
	public AboutFragmentItem(String title, String subtitle)
	{
		this.title = title;
		this.subtitle = subtitle;
	}

	/**
	 * Gets the title of this item.
	 * 
	 * @return the title of this item.
	 */
	public String getTitle()
	{
		return title;
	}

	/**
	 * Gets the subtitle of this item.
	 * 
	 * @return the subtitle of this item.
	 */
	public String getSubTitle()
	{
		return subtitle;
	}
}