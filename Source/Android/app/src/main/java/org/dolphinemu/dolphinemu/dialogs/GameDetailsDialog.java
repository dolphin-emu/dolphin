package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;

import java.io.File;


public final class GameDetailsDialog extends DialogFragment
{
	private GameFile mGameFile;

	public GameDetailsDialog(GameFile gameFile)
	{
		mGameFile = gameFile;
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState)
	{
		final GameFile gameFile = mGameFile;

		AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater().inflate(R.layout.dialog_game_details, null);

		String gamePath = gameFile.getPath();
		TextView textGameTitle = contents.findViewById(R.id.text_game_title);
		textGameTitle.setText(gamePath.substring(gamePath.lastIndexOf("/") + 1));

		Button buttonDeleteGame = contents.findViewById(R.id.button_delete_game);
		buttonDeleteGame.setOnClickListener(view ->
		{
			this.dismiss();
			this.deleteGameFile(getContext(), gameFile.getPath());
		});

		Button buttonDeleteSetting = contents.findViewById(R.id.button_delete_setting);
		buttonDeleteSetting.setOnClickListener(view ->
		{
			this.dismiss();
			this.deleteGameSetting(getContext(), gameFile.getGameId());
		});
		buttonDeleteSetting.setEnabled(isGameSetingExist(gameFile.getGameId()));

		Button buttonGameSetting = contents.findViewById(R.id.button_game_setting);
		buttonGameSetting.setOnClickListener(view ->
		{
			this.dismiss();
			SettingsActivity.launch(getActivity(), MenuTag.CONFIG, gameFile.getGameId());
		});

		Button buttonLaunch = contents.findViewById(R.id.button_launch);
		buttonLaunch.setOnClickListener(view ->
		{
			this.dismiss();
			EmulationActivity.launch(getActivity(), gameFile, -1);
		});

		ImageView imageGameScreen = contents.findViewById(R.id.image_game_screen);
		Picasso.with(imageGameScreen.getContext())
				.load("file://" + gameFile.getCoverPath())
				.into(imageGameScreen);

		builder.setView(contents);
		return builder.create();
	}

	private void deleteGameFile(Context context, String path)
	{
		File gameFile = new File(path);
		if(gameFile.exists())
		{
			if(gameFile.delete())
			{
				Toast.makeText(context, "Delete: " + path, Toast.LENGTH_SHORT).show();
				GameFileCacheService.startRescan(context);
			}
			else
			{
				Toast.makeText(context, "Error: " + path, Toast.LENGTH_SHORT).show();
			}
		}
		else
		{
			Toast.makeText(context, "No game file to delete", Toast.LENGTH_SHORT).show();
		}
	}

	private boolean isGameSetingExist(String gameId)
	{
		String path = DirectoryInitializationService.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
		File gameSettingsFile = new File(path);
		return gameSettingsFile.exists();
	}

	private void deleteGameSetting(Context context, String gameId)
	{
		String path = DirectoryInitializationService.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
		File gameSettingsFile = new File(path);
		if (gameSettingsFile.exists())
		{
			if (gameSettingsFile.delete())
			{
				Toast.makeText(context, "Cleared settings for " + gameId, Toast.LENGTH_SHORT).show();
			}
			else
			{
				Toast.makeText(context, "Unable to clear settings for " + gameId, Toast.LENGTH_SHORT).show();
			}
		}
		else
		{
			Toast.makeText(context, "No game settings to delete", Toast.LENGTH_SHORT).show();
		}
	}
}
