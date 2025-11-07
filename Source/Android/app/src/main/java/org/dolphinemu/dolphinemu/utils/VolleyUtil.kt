// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import com.android.volley.RequestQueue
import com.android.volley.toolbox.Volley
import org.dolphinemu.dolphinemu.DolphinApplication

object VolleyUtil {
  @JvmStatic
  val queue: RequestQueue by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
    Volley.newRequestQueue(DolphinApplication.getAppContext())
  }
}
