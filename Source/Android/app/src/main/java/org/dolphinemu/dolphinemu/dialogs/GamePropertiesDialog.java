// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.DolphinApplication;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.ConvertActivity;
import org.dolphinemu.dolphinemu.features.cheats.ui.CheatsActivity;
import org.dolphinemu.dolphinemu.features.riivolution.ui.RiivolutionBootActivity;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.ui.main.MainPresenter;
import org.dolphinemu.dolphinemu.ui.platform.Platform;
import org.dolphinemu.dolphinemu.utils.AlertDialogItemsBuilder;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.File;

public class GamePropertiesDialog extends DialogFragment
{
  public static final String TAG = "GamePropertiesDialog";
  private static final String ARG_PATH = "path";
  private static final String ARG_GAME_ID = "game_id";
  private static final String ARG_GAMETDB_ID = "gametdb_id";
  public static final String ARG_REVISION = "revision";
  public static final String ARG_DISC_NUMBER = "disc_number";
  private static final String ARG_PLATFORM = "platform";
  private static final String ARG_SHOULD_ALLOW_CONVERSION = "should_allow_conversion";

  public static GamePropertiesDialog newInstance(GameFile gameFile)
  {
    GamePropertiesDialog fragment = new GamePropertiesDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_PATH, gameFile.getPath());
    arguments.putString(ARG_GAME_ID, gameFile.getGameId());
    arguments.putString(ARG_GAMETDB_ID, gameFile.getGameTdbId());
    arguments.putInt(ARG_REVISION, gameFile.getRevision());
    arguments.putInt(ARG_DISC_NUMBER, gameFile.getDiscNumber());
    arguments.putInt(ARG_PLATFORM, gameFile.getPlatform());
    arguments.putBoolean(ARG_SHOULD_ALLOW_CONVERSION, gameFile.shouldAllowConversion());
    fragment.setArguments(arguments);

    return fragment;
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final String path = requireArguments().getString(ARG_PATH);
    final String gameId = requireArguments().getString(ARG_GAME_ID);
    final String gameTdbId = requireArguments().getString(ARG_GAMETDB_ID);
    final int revision = requireArguments().getInt(ARG_REVISION);
    final int discNumber = requireArguments().getInt(ARG_DISC_NUMBER);
    final int platform = requireArguments().getInt(ARG_PLATFORM);
    final boolean shouldAllowConversion =
            requireArguments().getBoolean(ARG_SHOULD_ALLOW_CONVERSION);

    final boolean isDisc = platform == Platform.GAMECUBE.toInt() ||
            platform == Platform.WII.toInt();
    final boolean isWii = platform != Platform.GAMECUBE.toInt();

    AlertDialogItemsBuilder itemsBuilder = new AlertDialogItemsBuilder(requireContext());

    itemsBuilder.add(R.string.properties_details, (dialog, i) ->
            GameDetailsDialog.newInstance(path).show(requireActivity()
                    .getSupportFragmentManager(), "game_details"));

    if (isDisc)
    {
      itemsBuilder.add(R.string.properties_start_with_riivolution, (dialog, i) ->
              RiivolutionBootActivity.launch(getContext(), path, gameId, revision, discNumber));

      itemsBuilder.add(R.string.properties_set_default_iso, (dialog, i) ->
      {
        try (Settings settings = new Settings())
        {
          settings.loadSettings();
          StringSetting.MAIN_DEFAULT_ISO.setString(settings, path);
          settings.saveSettings(getContext());
        }
      });
    }

    if (shouldAllowConversion)
    {
      itemsBuilder.add(R.string.properties_convert, (dialog, i) ->
              ConvertActivity.launch(getContext(), path));
    }

    if (isDisc && isWii)
    {
      itemsBuilder.add(R.string.properties_system_update, (dialog, i) ->
              MainPresenter.launchDiscUpdate(path, requireActivity()));
    }

    itemsBuilder.add(R.string.properties_edit_game_settings, (dialog, i) ->
            SettingsActivity.launch(getContext(), MenuTag.SETTINGS, gameId, revision, isWii));

    itemsBuilder.add(R.string.properties_edit_cheats, (dialog, i) ->
            CheatsActivity.launch(getContext(), gameId, gameTdbId, revision, isWii));

    itemsBuilder.add(R.string.properties_clear_game_settings, (dialog, i) ->
            clearGameSettingsWithConfirmation(gameId));

    MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(requireContext())
            .setTitle(requireContext()
                    .getString(R.string.preferences_game_properties_with_game_id, gameId));
    itemsBuilder.applyToBuilder(builder);
    return builder.create();
  }

  private void clearGameSettingsWithConfirmation(String gameId)
  {
    new MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.properties_clear_game_settings)
            .setMessage(R.string.properties_clear_game_settings_confirmation)
            .setPositiveButton(R.string.yes, (dialog, i) -> clearGameSettings(gameId))
            .setNegativeButton(R.string.no, null)
            .show();
  }

  private static void clearGameSettings(String gameId)
  {
    Context context = DolphinApplication.getAppContext();
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
        Toast.makeText(context,
                context.getResources().getString(R.string.properties_clear_success, gameId),
                Toast.LENGTH_SHORT).show();
      }
      else
      {
        Toast.makeText(context,
                context.getResources().getString(R.string.properties_clear_failure, gameId),
                Toast.LENGTH_SHORT).show();
      }
    }
    else
    {
      Toast.makeText(context, R.string.properties_clear_missing, Toast.LENGTH_SHORT).show();
    }
  }

  private static boolean recursivelyDeleteGameProfiles(@NonNull final File file, String gameId)
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
