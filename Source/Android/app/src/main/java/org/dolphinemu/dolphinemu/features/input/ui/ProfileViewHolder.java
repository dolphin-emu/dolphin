// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.ListItemProfileBinding;

public class ProfileViewHolder extends RecyclerView.ViewHolder
{
  private final ProfileDialogPresenter mPresenter;
  private final ListItemProfileBinding mBinding;

  private String mProfileName;
  private boolean mStock;

  public ProfileViewHolder(@NonNull ProfileDialogPresenter presenter,
          @NonNull ListItemProfileBinding binding)
  {
    super(binding.getRoot());

    mPresenter = presenter;
    mBinding = binding;

    binding.buttonLoad.setOnClickListener(view -> loadProfile());
    binding.buttonSave.setOnClickListener(view -> saveProfile());
    binding.buttonDelete.setOnClickListener(view -> deleteProfile());
  }

  public void bind(String profileName, boolean stock)
  {
    mProfileName = profileName;
    mStock = stock;

    mBinding.textName.setText(profileName);

    mBinding.buttonLoad.setVisibility(View.VISIBLE);
    mBinding.buttonSave.setVisibility(stock ? View.GONE : View.VISIBLE);
    mBinding.buttonDelete.setVisibility(stock ? View.GONE : View.VISIBLE);
  }

  public void bindAsEmpty(Context context)
  {
    mProfileName = null;
    mStock = false;

    mBinding.textName.setText(context.getText(R.string.input_profile_new));

    mBinding.buttonLoad.setVisibility(View.GONE);
    mBinding.buttonSave.setVisibility(View.VISIBLE);
    mBinding.buttonDelete.setVisibility(View.GONE);
  }

  private void loadProfile()
  {
    mPresenter.loadProfile(mProfileName, mStock);
  }

  private void saveProfile()
  {
    if (mProfileName == null)
      mPresenter.saveProfileAndPromptForName();
    else
      mPresenter.saveProfile(mProfileName);
  }

  private void deleteProfile()
  {
    mPresenter.deleteProfile(mProfileName);
  }
}
