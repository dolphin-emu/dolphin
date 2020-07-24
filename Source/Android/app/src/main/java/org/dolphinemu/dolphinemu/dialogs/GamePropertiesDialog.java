package org.dolphinemu.dolphinemu.dialogs;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import android.widget.Toast;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;

public class GamePropertiesDialog extends DialogFragment
{
  public static final String TAG = "GamePropertiesDialog";
  public static final String ARG_PATH = "path";
  public static final String ARG_GAMEID = "game_id";
  public static final String ARG_PLATFORM = "platform";

  public static GamePropertiesDialog newInstance(String path, String gameId, int platform)
  {
    GamePropertiesDialog fragment = new GamePropertiesDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_PATH, path);
    arguments.putString(ARG_GAMEID, gameId);
    arguments.putInt(ARG_PLATFORM, platform);
    fragment.setArguments(arguments);

    return fragment;
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    AlertDialog.Builder builder = new AlertDialog.Builder(requireContext(),
            R.style.DolphinDialogBase);

    String path = requireArguments().getString(ARG_PATH);
    String gameId = requireArguments().getString(ARG_GAMEID);
    int platform = requireArguments().getInt(ARG_PLATFORM);

    builder.setTitle(requireContext()
            .getString(R.string.preferences_game_properties) + ": " + gameId)
            .setItems(platform == Platform.GAMECUBE.toInt() ?
                    R.array.gameSettingsMenusGC :
                    R.array.gameSettingsMenusWii, (dialog, which) ->
            {
              switch (which)
              {
                case 0:
                  GameDetailsDialog.newInstance(path).show((requireActivity())
                          .getSupportFragmentManager(), "game_details");
                  break;
                case 1:
                  NativeLibrary.SetConfig(SettingsFile.FILE_NAME_DOLPHIN + ".ini",
                          Settings.SECTION_INI_CORE, SettingsFile.KEY_DEFAULT_ISO, path);
                  NativeLibrary.ReloadConfig();
                  Toast.makeText(getContext(), "Default ISO set", Toast.LENGTH_SHORT).show();
                  break;
                case 2:
                  SettingsActivity.launch(getContext(), MenuTag.CONFIG, gameId);
                  break;
                case 3:
                  SettingsActivity.launch(getContext(), MenuTag.GRAPHICS, gameId);
                  break;
                case 4:
                  SettingsActivity.launch(getContext(), MenuTag.GCPAD_TYPE, gameId);
                  break;
                case 5:
                  // Clear option for GC, Wii controls for else
                  if (platform == Platform.GAMECUBE.toInt())
                    clearGameSettings(gameId);
                  else
                    SettingsActivity.launch(getActivity(), MenuTag.WIIMOTE, gameId);
                  break;
                case 6:
                  clearGameSettings(gameId);
                  break;
              }
            });
    return builder.create();
  }


  private void clearGameSettings(String gameId)
  {
    String gameSettingsPath =
            DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
    String gameProfilesPath = DirectoryInitialization.getUserDirectory() + "/Config/Profiles/";
    File gameSettingsFile = new File(gameSettingsPath);
    File gameProfilesDirectory = new File(gameProfilesPath);
    boolean hadGameProfiles = recursivelyDeleteGameProfiles(gameProfilesDirectory, gameId);

    if (gameSettingsFile.exists() || hadGameProfiles)
    {
      if (gameSettingsFile.delete() || hadGameProfiles)
      {
        Toast.makeText(getContext(), "Cleared settings for " + gameId, Toast.LENGTH_SHORT)
                .show();
      }
      else
      {
        Toast.makeText(getContext(), "Unable to clear settings for " + gameId,
                Toast.LENGTH_SHORT).show();
      }
    }
    else
    {
      Toast.makeText(getContext(), "No game settings to delete", Toast.LENGTH_SHORT).show();
    }
  }

  private boolean recursivelyDeleteGameProfiles(@NonNull final File file, String gameId)
  {
    boolean hadGameProfiles = false;

    if (file.isDirectory())
    {
      File[] files = file.listFiles();

      if (files == null)
      {
        return false;
      }

      for (File child : files)
      {
        if (child.getName().startsWith(gameId) && child.isFile())
        {
          if (!child.delete())
          {
            Log.error("[GamePropertiesDialog] Failed to delete " + child.getAbsolutePath());
          }
          hadGameProfiles = true;
        }
        hadGameProfiles |= recursivelyDeleteGameProfiles(child, gameId);
      }
    }
    return hadGameProfiles;
  }
}
