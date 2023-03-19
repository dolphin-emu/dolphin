// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.databinding.ListItemProfileBinding;

public final class ProfileAdapter extends RecyclerView.Adapter<ProfileViewHolder>
{
  private final Context mContext;
  private final ProfileDialogPresenter mPresenter;

  private final String[] mStockProfileNames;
  private final String[] mUserProfileNames;

  public ProfileAdapter(Context context, ProfileDialogPresenter presenter)
  {
    mContext = context;
    mPresenter = presenter;

    mStockProfileNames = presenter.getProfileNames(true);
    mUserProfileNames = presenter.getProfileNames(false);
  }

  @NonNull @Override
  public ProfileViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());

    ListItemProfileBinding binding = ListItemProfileBinding.inflate(inflater, parent, false);
    return new ProfileViewHolder(mPresenter, binding);
  }

  @Override
  public void onBindViewHolder(@NonNull ProfileViewHolder holder, int position)
  {
    if (position < mStockProfileNames.length)
    {
      holder.bind(mStockProfileNames[position], true);
      return;
    }

    position -= mStockProfileNames.length;

    if (position < mUserProfileNames.length)
    {
      holder.bind(mUserProfileNames[position], false);
      return;
    }

    holder.bindAsEmpty(mContext);
  }

  @Override
  public int getItemCount()
  {
    return mStockProfileNames.length + mUserProfileNames.length + 1;
  }
}
