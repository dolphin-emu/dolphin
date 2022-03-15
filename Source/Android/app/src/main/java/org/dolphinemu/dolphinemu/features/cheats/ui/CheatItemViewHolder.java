// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

public abstract class CheatItemViewHolder extends RecyclerView.ViewHolder
{
  public CheatItemViewHolder(@NonNull View itemView)
  {
    super(itemView);
  }

  public abstract void bind(CheatsActivity activity, CheatItem item, int position);
}
