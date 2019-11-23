package org.dolphinemu.dolphinemu.dialogs;

import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.File;

public class GameSettingsDialog extends DialogFragment
{
  public static final String TAG = "GameSettingsDialog";
  public static final String ARG_GAMEID = "game_id";
  public static final String ARG_PLATFORM = "platform";

  public static GameSettingsDialog newInstance(String gameId, int platform)
  {
    GameSettingsDialog fragment = new GameSettingsDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_GAMEID, gameId);
    arguments.putInt(ARG_PLATFORM, platform);
    fragment.setArguments(arguments);

    return fragment;
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());

    String gameId = requireArguments().getString(ARG_GAMEID);
    int platform = requireArguments().getInt(ARG_PLATFORM);

    builder.setTitle(requireContext().getString(R.string.preferences_game_settings) + ": " + gameId)
            .setItems(platform == Platform.GAMECUBE.toInt() ?
                    R.array.gameSettingsMenusGC :
                    R.array.gameSettingsMenusWii, (dialog, which) ->
            {
              switch (which)
              {
                case 0:
                  SettingsActivity.launch(getContext(), MenuTag.CONFIG, gameId);
                  break;
                case 1:
                  SettingsActivity.launch(getContext(), MenuTag.GRAPHICS, gameId);
                  break;
                case 2:
                  SettingsActivity.launch(getContext(), MenuTag.GCPAD_TYPE, gameId);
                  break;
                case 3:
                  // Clear option for GC, Wii controls for else
                  if (platform == Platform.GAMECUBE.toInt())
                    clearGameSettings(gameId);
                  else
                    SettingsActivity.launch(getActivity(), MenuTag.WIIMOTE, gameId);
                  break;
                case 4:
                  clearGameSettings(gameId);
                  break;
              }
            });
    return builder.create();
  }


  private void clearGameSettings(String gameId)
  {
    String path =
            DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
    File gameSettingsFile = new File(path);
    if (gameSettingsFile.exists())
    {
      if (gameSettingsFile.delete())
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
}
