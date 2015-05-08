package org.dolphinemu.dolphinemu.model;

public interface Game
{
	public static final int PLATFORM_GC = 0;
	public static final int PLATFORM_WII = 1;

	public int getPlatform();

	public String getDate();

	public String getTitle();

	public String getDescription();

	public String getCountry();

	public String getPath();

	public String getGameId();

	public String getScreenPath();
}
