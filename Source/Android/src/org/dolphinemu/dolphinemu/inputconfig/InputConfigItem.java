/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.inputconfig;

import org.dolphinemu.dolphinemu.NativeLibrary;

/**
 * Represents a controller input item (button, stick, etc).
 */
public final class InputConfigItem implements Comparable<InputConfigItem>
{
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

	/**
	 * Constructor
	 * 
	 * @param name        Name of the input config item.
	 * @param config      Name of the key in the configuration file that this control modifies.
	 * @param defaultBind Default binding to fall back upon if binding fails.
	 */
	public InputConfigItem(String name, String config, String defaultBind)
	{
		Init(name, config, defaultBind);
	}

	/**
	 * Constructor that creates an InputConfigItem
	 * that has a default binding of "None"
	 * 
	 * @param name   Name of the input config item.
	 * @param config Name of the key in the configuration file that this control modifies.
	 */
	public InputConfigItem(String name, String config)
	{
		Init(name, config, "None");
	}
	
	/**
	 * Gets the name of this InputConfigItem.
	 * 
	 * @return the name of this InputConfigItem
	 */
	public String getName()
	{
		return m_name;
	}
	
	/**
	 * Gets the config key this InputConfigItem modifies.
	 * 
	 * @return the config key this InputConfigItem modifies.
	 */
	public String getConfig()
	{
		return m_Config;
	}
	
	/**
	 * Gets the currently set binding of this InputConfigItem
	 * 
	 * @return the currently set binding of this InputConfigItem
	 */
	public String getBind()
	{
		return m_bind;
	}
	
	/**
	 * Sets a new binding for this InputConfigItem.
	 * 
	 * @param bind The new binding.
	 */
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
