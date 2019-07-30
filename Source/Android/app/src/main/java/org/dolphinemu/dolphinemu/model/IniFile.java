package org.dolphinemu.dolphinemu.model;

public class IniFile
{
  // Do not rename or move without editing the native code
  private long mPointer;

  public IniFile()
  {
    mPointer = newIniFile();
  }

  private static native long newIniFile();

  @Override
  public native void finalize();

  public native boolean loadFile(String filename, boolean keep_current_data);
  public native boolean saveFile(String filename);

  public native void setString(String section, String key, String value);
  public native String getString(String section, String key, String default_value);

  public native boolean delete(String section, String key);
}
