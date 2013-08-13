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

	private void Init(String name, String config, String defaultBind)
	{
		m_name = name;
		m_Config = config;
		String ConfigValues[] = m_Config.split("-");
		String Key = ConfigValues[0];
		String Value = ConfigValues[1];
		m_bind = NativeLibrary.GetConfig("Dolphin.ini", Key, Value, defaultBind);
	}

	public InputConfigItem(String name, String config, String defaultBind)
	{
		Init(name, config, defaultBind);
	}

	public InputConfigItem(String name, String config)
	{
		Init(name, config, "None");
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
	public void setBind(String bind)
	{
		m_bind = bind;
	}

	public int compareTo(InputConfigItem o)
	{
		if(this.m_name != null)
			return this.m_name.toLowerCase().compareTo(o.getName().toLowerCase());
		else
			throw new IllegalArgumentException();
	}
}
