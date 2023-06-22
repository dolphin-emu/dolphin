// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import androidx.annotation.Keep;

import java.io.File;

// An in-memory copy of an INI file
public class IniFile
{
  // This class is non-static to ensure that the IniFile parent does not get garbage collected
  // while a section still is accessible. (The finalizer of IniFile deletes the native sections.)
  @SuppressWarnings("InnerClassMayBeStatic")
  public class Section
  {
    @Keep
    private long mPointer;

    @Keep
    private Section(long pointer)
    {
      mPointer = pointer;
    }

    public native boolean exists(String key);

    public native boolean delete(String key);

    public native String getString(String key, String defaultValue);

    public native boolean getBoolean(String key, boolean defaultValue);

    public native int getInt(String key, int defaultValue);

    public native float getFloat(String key, float defaultValue);

    public native void setString(String key, String newValue);

    public native void setBoolean(String key, boolean newValue);

    public native void setInt(String key, int newValue);

    public native void setFloat(String key, float newFloat);
  }

  @Keep
  private long mPointer;

  public IniFile()
  {
    mPointer = newIniFile();
  }

  public IniFile(IniFile other)
  {
    mPointer = copyIniFile(other);
  }

  public IniFile(String path)
  {
    this();
    load(path, false);
  }

  public IniFile(File file)
  {
    this();
    load(file, false);
  }

  public native boolean load(String path, boolean keepCurrentData);

  public boolean load(File file, boolean keepCurrentData)
  {
    return load(file.getPath(), keepCurrentData);
  }

  public native boolean save(String path);

  public boolean save(File file)
  {
    return save(file.getPath());
  }

  public native Section getOrCreateSection(String sectionName);

  public native boolean exists(String sectionName);

  public native boolean exists(String sectionName, String key);

  public native boolean deleteSection(String sectionName);

  public native boolean deleteKey(String sectionName, String key);

  public native String getString(String sectionName, String key, String defaultValue);

  public native boolean getBoolean(String sectionName, String key, boolean defaultValue);

  public native int getInt(String sectionName, String key, int defaultValue);

  public native float getFloat(String sectionName, String key, float defaultValue);

  public native void setString(String sectionName, String key, String newValue);

  public native void setBoolean(String sectionName, String key, boolean newValue);

  public native void setInt(String sectionName, String key, int newValue);

  public native void setFloat(String sectionName, String key, float newValue);

  @Override
  public native void finalize();

  private native long newIniFile();

  private native long copyIniFile(IniFile other);
}
