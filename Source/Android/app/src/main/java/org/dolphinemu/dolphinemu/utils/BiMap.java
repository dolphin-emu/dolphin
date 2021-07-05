// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import java.util.HashMap;
import java.util.Map;

public class BiMap<K, V>
{

  private Map<K, V> forward = new HashMap<>();
  private Map<V, K> backward = new HashMap<>();

  public synchronized void add(K key, V value)
  {
    forward.put(key, value);
    backward.put(value, key);
  }

  public synchronized V getForward(K key)
  {
    return forward.get(key);
  }

  public synchronized K getBackward(V key)
  {
    return backward.get(key);
  }
}
