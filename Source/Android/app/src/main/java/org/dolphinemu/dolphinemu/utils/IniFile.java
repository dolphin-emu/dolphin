package org.dolphinemu.dolphinemu.utils;

import java.io.File;

// An in-memory copy of an INI file
public class IniFile
{
  // This class is non-static to ensure that the IniFile parent does not get garbage collected
  // while a section still is accessible. (The finalizer of IniFile deletes the native sections.)
  public class Section
  {
    private long mPointer;  // Do not rename or move without editing the native code

    private Section(long pointer)
    {
      mPointer = pointer;
    }
  }

  private long mPointer;  // Do not rename or move without editing the native code

  public IniFile()
  {
    mPointer = newIniFile();
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

  public native String getString(String sectionName, String key, String defaultValue);

  public native boolean getBoolean(String sectionName, String key, boolean defaultValue);

  public native int getInt(String sectionName, String key, int defaultValue);

  public native void setString(String sectionName, String key, String newValue);

  public native void setBoolean(String sectionName, String key, boolean newValue);

  public native void setInt(String sectionName, String key, int newValue);

  @Override
  public native void finalize();

  private native long newIniFile();
}
