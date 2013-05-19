package org.dolphinemu.dolphinemu;

public class SettingMenuItem implements Comparable<SettingMenuItem>{
    private String name;
    private String subtitle;
    private int m_id;

    public SettingMenuItem(String n,String d, int id)
    {
        name = n;
        subtitle = d;
        m_id = id;
    }

    public String getName()
    {
        return name;
    }

    public String getSubtitle()
    {
        return subtitle;
    }
    public int getID()
    {
        return m_id;
    }

    public int compareTo(SettingMenuItem o)
    {
        if(this.name != null)
            return this.name.toLowerCase().compareTo(o.getName().toLowerCase());
        else
            throw new IllegalArgumentException();
    }
}

