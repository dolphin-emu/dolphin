// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders;

import android.util.Pair;

import org.dolphinemu.dolphinemu.features.skylanders.model.SkylanderPair;

import java.util.Map;

public class SkylanderConfig
{
  public static final Map<SkylanderPair, String> LIST_SKYLANDERS;
  public static final Map<String, SkylanderPair> REVERSE_LIST_SKYLANDERS;

  static
  {
    LIST_SKYLANDERS = getSkylanderMap();
    REVERSE_LIST_SKYLANDERS = getInverseSkylanderMap();
  }

  public static native Map<SkylanderPair, String> getSkylanderMap();

  public static native Map<String, SkylanderPair> getInverseSkylanderMap();

  public static native boolean removeSkylander(int slot);

  public static native Pair<Integer, String> loadSkylander(int slot, String fileName);

  public static native Pair<Integer, String> createSkylander(int id, int var, String fileName,
          int slot);
}
