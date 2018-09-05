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


public final class GameDetailsDialog extends DialogFragment {
	private static final String ARG_GAME_PATH = "game_path";

	public static GameDetailsDialog newInstance(String gamePath) {
		GameDetailsDialog fragment = new GameDetailsDialog();

		Bundle arguments = new Bundle();
		arguments.putString(ARG_GAME_PATH, gamePath);
		fragment.setArguments(arguments);

		return fragment;
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		final GameFile gameFile = GameFileCacheService.addOrGet(getArguments().getString(ARG_GAME_PATH));

		AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater().inflate(R.layout.dialog_game_details, null);

		// game title
		TextView textGameTitle = contents.findViewById(R.id.text_game_title);
		textGameTitle.setText(gameFile.getTitle());

		// game filename
		String gamePath = gameFile.getPath();
		TextView textGameFilename = contents.findViewById(R.id.text_game_filename);
		textGameFilename.setText(gamePath.substring(gamePath.lastIndexOf("/") + 1));

		Button buttonDeleteSetting = contents.findViewById(R.id.button_delete_setting);
		buttonDeleteSetting.setOnClickListener(view ->
		{
			this.dismiss();
			this.deleteGameSetting(getContext(), gameFile.getGameId());
		});
		buttonDeleteSetting.setEnabled(isGameSetingExist(gameFile.getGameId()));

		Button buttonARCode = contents.findViewById(R.id.button_action_replay);
		buttonARCode.setOnClickListener(view ->
		{
			this.dismiss();
			CheatCodeDialog.newInstance(gamePath, CheatCodeDialog.CODE_TYPE_AR).show(getFragmentManager(), "CheatCodeDialog");
		});

		Button buttonGecko = contents.findViewById(R.id.button_gecko_code);
		buttonGecko.setOnClickListener(view ->
		{
			this.dismiss();
			CheatCodeDialog.newInstance(gamePath, CheatCodeDialog.CODE_TYPE_GECKO).show(getFragmentManager(), "CheatCodeDialog");
		});

		Button buttonGameSetting = contents.findViewById(R.id.button_game_setting);
		buttonGameSetting.setOnClickListener(view ->
		{
			this.dismiss();
			SettingsActivity.launch(getActivity(), MenuTag.CONFIG, gameFile.getGameId());
		});

		Button buttonLaunch = contents.findViewById(R.id.button_quick_load);
		buttonLaunch.setOnClickListener(view ->
		{
			this.dismiss();
			EmulationActivity.launch(getActivity(), gameFile, gameFile.getLastSavedState());
		});

		ImageView imageGameScreen = contents.findViewById(R.id.image_game_screen);
		Picasso.with(imageGameScreen.getContext())
			.load("file://" + gameFile.getCoverPath())
			.into(imageGameScreen);

		builder.setView(contents);
		return builder.create();
	}

	private boolean isGameSetingExist(String gameId) {
		String path = DirectoryInitializationService.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
		File gameSettingsFile = new File(path);
		return gameSettingsFile.exists();
	}

	private void deleteGameSetting(Context context, String gameId) {
		String path = DirectoryInitializationService.getUserDirectory() + "/GameSettings/" + gameId + ".ini";
		File gameSettingsFile = new File(path);
		if (gameSettingsFile.exists()) {
			if (gameSettingsFile.delete()) {
				Toast.makeText(context, "Cleared settings for " + gameId, Toast.LENGTH_SHORT).show();
			} else {
				Toast.makeText(context, "Unable to clear settings for " + gameId, Toast.LENGTH_SHORT).show();
			}
		} else {
			Toast.makeText(context, "No game settings to delete", Toast.LENGTH_SHORT).show();
		}
	}
}
