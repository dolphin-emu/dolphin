package org.dolphinemu.dolphinemu.utils;

import android.os.Environment;
import android.support.annotation.NonNull;

import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.FloatSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.StringSetting;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Set;

public final class SettingsFile
{
	private SettingsFile()
	{
	}

	public static HashMap<String, SettingSection> readFile(String fileName)
	{
		HashMap<String, SettingSection> sections = new HashMap<>();

		File ini = getSettingsFile(fileName);

		BufferedReader reader = null;

		try
		{
			reader = new BufferedReader(new FileReader(ini));

			SettingSection current = null;
			for (String line; (line = reader.readLine()) != null; )
			{
				if (line.startsWith("[") && line.endsWith("]"))
				{
					current = sectionFromLine(line);
					sections.put(current.getName(), current);
				}
				else if ((current != null) && line.contains("="))
				{
					Setting setting = settingFromLine(current, line);
					current.putSetting(setting.getKey(), setting);
				}
			}
		}
		catch (FileNotFoundException e)
		{
			Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.getMessage());
		}
		catch (IOException e)
		{
			Log.error("[SettingsFile] Error reading from: " + fileName + ".ini: " + e.getMessage());
		} finally
		{
			if (reader != null)
			{
				try
				{
					reader.close();
				}
				catch (IOException e)
				{
					Log.error("[SettingsFile] Error closing: " + fileName + ".ini: " + e.getMessage());
				}
			}
		}

		return sections;
	}

	@NonNull
	private static File getSettingsFile(String fileName)
	{
		String storagePath = Environment.getExternalStorageDirectory().getAbsolutePath();
		return new File(storagePath + "/dolphin-emu/Config/" + fileName + ".ini");
	}

	public static void saveFile(String fileName, HashMap<String, SettingSection> sections)
	{
		File ini = getSettingsFile(fileName);

		PrintWriter writer = null;
		try
		{
			writer = new PrintWriter(ini, "UTF-8");

			Set<String> keySet = sections.keySet();

			for (String key : keySet)
			{
				SettingSection section = sections.get(key);
				writeSection(writer, section);
			}
		}
		catch (FileNotFoundException e)
		{
			Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.getMessage());
		}
		catch (UnsupportedEncodingException e)
		{
			Log.error("[SettingsFile] Bad encoding; please file a bug report: " + fileName + ".ini: " + e.getMessage());
		} finally
		{
			if (writer != null)
			{
				writer.close();
			}
		}
	}

	private static SettingSection sectionFromLine(String line)
	{
		String sectionName = line.substring(1, line.length() - 1);
		return new SettingSection(sectionName);
	}

	private static Setting settingFromLine(SettingSection current, String line)
	{
		String[] splitLine = line.split("=");

		String key = splitLine[0].trim();
		String value = splitLine[1].trim();

		try
		{
			int valueAsInt = Integer.valueOf(value);

			return new IntSetting(key, current, valueAsInt);
		}
		catch (NumberFormatException ex)
		{
		}

		try
		{
			float valueAsFloat = Float.valueOf(value);

			return new FloatSetting(key, current, valueAsFloat);
		}
		catch (NumberFormatException ex)
		{
		}

		switch (value)
		{
			case "True":
				return new BooleanSetting(key, current, true);
			case "False":
				return new BooleanSetting(key, current, false);
			default:
				return new StringSetting(key, current, value);
		}
	}

	private static void writeSection(PrintWriter writer, SettingSection section)
	{
		// Write the section header.
		String header = sectionAsString(section);
		writer.println(header);

		// Write this section's values.
		HashMap<String, Setting> settings = section.getSettings();
		Set<String> keySet = settings.keySet();

		for (String key : keySet)
		{
			Setting setting = settings.get(key);
			String settingString = settingAsString(setting);

			writer.println(settingString);
		}
	}

	private static String sectionAsString(SettingSection section)
	{
		return "[" + section.getName() + "]";
	}

	private static String settingAsString(Setting setting)
	{
		return setting.getKey() + " = " + setting.getValueAsString();
	}
}
