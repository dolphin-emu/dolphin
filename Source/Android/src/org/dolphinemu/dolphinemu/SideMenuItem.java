package org.dolphinemu.dolphinemu;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

public class SideMenuItem implements Comparable<SideMenuItem>{
    private String m_name;
    private int m_id;

    public SideMenuItem(String n, int id)
    {
        m_name = n;
        m_id = id;
    }
    
    public String getName()
    {
        return m_name;
    }
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

