/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.sidemenu;


/**
 * Represents an item that goes in the sidemenu of the app.
 */
public final class SideMenuItem
{
	private final String name;
	private final int id;

	/**
	 * Constructor
	 * 
	 * @param name The name of the SideMenuItem.
	 * @param id   ID number of this specific SideMenuItem.
	 */
	public SideMenuItem(String name, int id)
	{
		this.name = name;
		this.id = id;
	}

	/**
	 * Gets the name of this SideMenuItem.
	 * 
	 * @return the name of this SideMenuItem.
	 */
	public String getName()
	{
		return name;
	}

	/**
	 * Gets the ID of this SideMenuItem.
	 * 
	 * @return the ID of this SideMenuItem.
	 */
	public int getID()
	{
		return id;
	}
}

