// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.content.Context;
import android.content.Intent;

import androidx.appcompat.app.AppCompatActivity;

public class CheatsActivity extends AppCompatActivity
{
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_REVISION = "revision";

  public static void launch(Context context, String gameId, int revision)
  {
    Intent intent = new Intent(context, CheatsActivity.class);
    intent.putExtra(ARG_GAME_ID, gameId);
    intent.putExtra(ARG_REVISION, revision);
    context.startActivity(intent);
  }
}
