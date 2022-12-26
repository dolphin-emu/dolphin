// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.DialogInputStringBinding;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivityView;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.File;
import java.text.Collator;
import java.util.Arrays;

public final class ProfileDialogPresenter
{
  private static final String EXTENSION = ".ini";

  private final Context mContext;
  private final DialogFragment mDialog;
  private final MenuTag mMenuTag;

  public ProfileDialogPresenter(MenuTag menuTag)
  {
    mContext = null;
    mDialog = null;
    mMenuTag = menuTag;
  }

  public ProfileDialogPresenter(DialogFragment dialog, MenuTag menuTag)
  {
    mContext = dialog.getContext();
    mDialog = dialog;
    mMenuTag = menuTag;
  }

  public String[] getProfileNames(boolean stock)
  {
    File[] profiles = new File(getProfileDirectoryPath(stock)).listFiles(
            file -> !file.isDirectory() && file.getName().endsWith(EXTENSION));

    if (profiles == null)
      return new String[0];

    return Arrays.stream(profiles)
            .map(file -> file.getName().substring(0, file.getName().length() - EXTENSION.length()))
            .sorted(Collator.getInstance())
            .toArray(String[]::new);
  }

  public void loadProfile(@NonNull String profileName, boolean stock)
  {
    new MaterialAlertDialogBuilder(mContext)
            .setMessage(mContext.getString(R.string.input_profile_confirm_load, profileName))
            .setPositiveButton(R.string.yes, (dialogInterface, i) ->
            {
              mMenuTag.getCorrespondingEmulatedController()
                      .loadProfile(getProfilePath(profileName, stock));
              ((SettingsActivityView) mDialog.requireActivity()).onControllerSettingsChanged();
              mDialog.dismiss();
            })
            .setNegativeButton(R.string.no, null)
            .show();
  }

  public void saveProfile(@NonNull String profileName)
  {
    // If the user is saving over an existing profile, we should show an overwrite warning.
    // If the user is creating a new profile, we normally shouldn't show a warning,
    // but if they've entered the name of an existing profile, we should shown an overwrite warning.

    String profilePath = getProfilePath(profileName, false);
    if (!new File(profilePath).exists())
    {
      mMenuTag.getCorrespondingEmulatedController().saveProfile(profilePath);
      mDialog.dismiss();
    }
    else
    {
      new MaterialAlertDialogBuilder(mContext)
              .setMessage(mContext.getString(R.string.input_profile_confirm_save, profileName))
              .setPositiveButton(R.string.yes, (dialogInterface, i) ->
              {
                mMenuTag.getCorrespondingEmulatedController().saveProfile(profilePath);
                mDialog.dismiss();
              })
              .setNegativeButton(R.string.no, null)
              .show();
    }
  }

  public void saveProfileAndPromptForName()
  {
    LayoutInflater inflater = LayoutInflater.from(mContext);

    DialogInputStringBinding binding = DialogInputStringBinding.inflate(inflater);
    TextInputEditText input = binding.input;

    new MaterialAlertDialogBuilder(mContext)
            .setView(binding.getRoot())
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
                    saveProfile(input.getText().toString()))
            .setNegativeButton(R.string.cancel, null)
            .show();
  }

  public void deleteProfile(@NonNull String profileName)
  {
    new MaterialAlertDialogBuilder(mContext)
            .setMessage(mContext.getString(R.string.input_profile_confirm_delete, profileName))
            .setPositiveButton(R.string.yes, (dialogInterface, i) ->
            {
              new File(getProfilePath(profileName, false)).delete();
              mDialog.dismiss();
            })
            .setNegativeButton(R.string.no, null)
            .show();
  }

  private String getProfileDirectoryName()
  {
    if (mMenuTag.isGCPadMenu())
      return "GCPad";
    else if (mMenuTag.isWiimoteMenu())
      return "Wiimote";
    else
      throw new UnsupportedOperationException();
  }

  private String getProfileDirectoryPath(boolean stock)
  {
    if (stock)
    {
      return DirectoryInitialization.getSysDirectory() + "/Profiles/" + getProfileDirectoryName() +
              '/';
    }
    else
    {
      return DirectoryInitialization.getUserDirectory() + "/Config/Profiles/" +
              getProfileDirectoryName() + '/';
    }
  }

  private String getProfilePath(String profileName, boolean stock)
  {
    return getProfileDirectoryPath(stock) + profileName + EXTENSION;
  }
}
