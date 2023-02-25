// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.content.Context;
import android.content.DialogInterface.OnClickListener;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;

public class AlertDialogItemsBuilder
{
  private Context mContext;

  private ArrayList<CharSequence> mLabels = new ArrayList<>();
  private ArrayList<OnClickListener> mListeners = new ArrayList<>();

  public AlertDialogItemsBuilder(Context context)
  {
    mContext = context;
  }

  public void add(int stringId, OnClickListener listener)
  {
    mLabels.add(mContext.getResources().getString(stringId));
    mListeners.add(listener);
  }

  public void add(CharSequence label, OnClickListener listener)
  {
    mLabels.add(label);
    mListeners.add(listener);
  }

  public void applyToBuilder(MaterialAlertDialogBuilder builder)
  {
    CharSequence[] labels = new CharSequence[mLabels.size()];
    labels = mLabels.toArray(labels);
    builder.setItems(labels, (dialog, i) -> mListeners.get(i).onClick(dialog, i));
  }
}
