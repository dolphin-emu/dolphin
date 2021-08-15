// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;

import com.android.volley.RequestQueue;
import com.android.volley.toolbox.Volley;

public class VolleyUtil
{
  private static RequestQueue queue;

  public static void init(Context context)
  {
    if (queue == null)
      queue = Volley.newRequestQueue(context);
  }

  public static RequestQueue getQueue()
  {
    return queue;
  }
}
