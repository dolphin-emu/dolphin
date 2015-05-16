package org.dolphinemu.dolphinemu.model;

public interface Game
{
	public static final int PLATFORM_GC = 0;
	public static final int PLATFORM_WII = 1;
	public static final int PLATFORM_WII_WARE = 2;

	// Copied from IVolume::ECountry. Update these if that is ever modified.
	public static final int COUNTRY_EUROPE = 0;
	public static final int COUNTRY_JAPAN = 1;
	public static final int COUNTRY_USA = 2;
	public static final int COUNTRY_AUSTRALIA = 3;
	public static final int COUNTRY_FRANCE = 4;
	public static final int COUNTRY_GERMANY = 5;
	public static final int COUNTRY_ITALY = 6;
	public static final int COUNTRY_KOREA = 7;
	public static final int COUNTRY_NETHERLANDS = 8;
	public static final int COUNTRY_RUSSIA = 9;
	public static final int COUNTRY_SPAIN = 10;
	public static final int COUNTRY_TAIWAN = 11;
	public static final int COUNTRY_WORLD = 12;
	public static final int COUNTRY_UNKNOWN = 13;

	public int getPlatform();

	public String getDate();

	public String getTitle();

	public String getDescription();

	public int getCountry();

	public String getPath();

	public String getGameId();

	public String getScreenPath();
}
