package org.dolphinemu.dolphinemu;

/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */
public class InputConfigItem implements Comparable<InputConfigItem>{
	private String m_name;
	private String m_Config;
	private String m_bind;

	private void Init(String n, String c, String d)
	{
		m_name = n;
		m_Config = c;
		String ConfigValues[] = m_Config.split("-");
		String Key = ConfigValues[0];
		String Value = ConfigValues[1];
		m_bind = NativeLibrary.GetConfig("Dolphin.ini", Key, Value, d);
	}

	public InputConfigItem(String n, String c, String d)
	{
		Init(n, c, d);
	}

	public InputConfigItem(String n, String c)
	{
		Init(n, c, "None");
	}
	public String getName()
	{
		return m_name;
	}
	public String getConfig()
	{
		return m_Config;
	}
	public String getBind()
	{
		return m_bind;
	}
	public void setBind(String b)
	{
		m_bind = b;
	}

	public int compareTo(InputConfigItem o)
	{
		if(this.m_name != null)
			return this.m_name.toLowerCase().compareTo(o.getName().toLowerCase());
		else
			throw new IllegalArgumentException();
	}
}
