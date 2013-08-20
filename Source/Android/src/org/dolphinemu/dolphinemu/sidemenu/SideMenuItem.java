/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.sidemenu;


/**
 * Represents an item that goes in the sidemenu of the app.
 */
public final class SideMenuItem implements Comparable<SideMenuItem>
{
    private final String m_name;
    private final int m_id;

    /**
     * Constructor
     * 
     * @param name The name of the SideMenuItem.
     * @param id   ID number of this specific SideMenuItem.
     */
    public SideMenuItem(String name, int id)
    {
        m_name = name;
        m_id = id;
    }
    
    /**
     * Gets the name of this SideMenuItem.
     * 
     * @return the name of this SideMenuItem.
     */
    public String getName()
    {
        return m_name;
    }
    
    /**
     * Gets the ID of this SideMenuItem.
     * 
     * @return the ID of this SideMenuItem.
     */
    public int getID()
    {
    	return m_id;
    }
    
    public int compareTo(SideMenuItem o) 
    {
        if(this.m_name != null)
            return this.m_name.toLowerCase().compareTo(o.getName().toLowerCase()); 
        else 
            throw new IllegalArgumentException();
    }
}

