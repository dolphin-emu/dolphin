package org.dolphinemu.dolphinemu.dialogs;


import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.emulation.EmulationActivity;
import org.dolphinemu.dolphinemu.model.Game;

import de.hdodenhof.circleimageview.CircleImageView;

public class GameDetailsDialog extends DialogFragment
{
	public static final String ARGUMENT_GAME_TITLE = BuildConfig.APPLICATION_ID + ".game_title";
	public static final String ARGUMENT_GAME_DESCRIPTION = BuildConfig.APPLICATION_ID + ".game_description";
	public static final String ARGUMENT_GAME_COUNTRY = BuildConfig.APPLICATION_ID + ".game_country";
	public static final String ARGUMENT_GAME_DATE = BuildConfig.APPLICATION_ID + ".game_date";
	public static final String ARGUMENT_GAME_PATH = BuildConfig.APPLICATION_ID + ".game_path";
	public static final String ARGUMENT_GAME_SCREENSHOT_PATH = BuildConfig.APPLICATION_ID + ".game_screenshot_path";


	public static GameDetailsDialog newInstance(Game game)
	{
		GameDetailsDialog fragment = new GameDetailsDialog();

		Bundle arguments = new Bundle();
		arguments.putString(ARGUMENT_GAME_TITLE, game.getTitle());
		arguments.putString(ARGUMENT_GAME_DESCRIPTION, game.getDescription());
		arguments.putString(ARGUMENT_GAME_COUNTRY, game.getCountry());
		arguments.putString(ARGUMENT_GAME_DATE, game.getDate());
		arguments.putString(ARGUMENT_GAME_PATH, game.getPath());
		arguments.putString(ARGUMENT_GAME_SCREENSHOT_PATH, game.getScreenPath());
		fragment.setArguments(arguments);

		return fragment;
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState)
	{
		AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater().inflate(R.layout.dialog_game_details, null);

		final ImageView imageGameScreen = (ImageView) contents.findViewById(R.id.image_game_screen);
		CircleImageView circleBanner = (CircleImageView) contents.findViewById(R.id.circle_banner);

		TextView textTitle = (TextView) contents.findViewById(R.id.text_game_title);
		TextView textDescription = (TextView) contents.findViewById(R.id.text_game_description);

		TextView textCountry = (TextView) contents.findViewById(R.id.text_country);
		TextView textDate = (TextView) contents.findViewById(R.id.text_date);

		ImageButton buttonLaunch = (ImageButton) contents.findViewById(R.id.button_launch);

		textTitle.setText(getArguments().getString(ARGUMENT_GAME_TITLE));
		textDescription.setText(getArguments().getString(ARGUMENT_GAME_DESCRIPTION));
		textCountry.setText(getArguments().getString(ARGUMENT_GAME_COUNTRY));
		textDate.setText(getArguments().getString(ARGUMENT_GAME_DATE));
		buttonLaunch.setOnClickListener(new View.OnClickListener()
		{
			@Override
			public void onClick(View view)
			{
				// Start the emulation activity and send the path of the clicked ROM to it.
				Intent intent = new Intent(view.getContext(), EmulationActivity.class);

				intent.putExtra("SelectedGame", getArguments().getString(ARGUMENT_GAME_PATH));
				startActivity(intent);
			}
		});

		// Fill in the view contents.
		Picasso.with(imageGameScreen.getContext())
				.load(getArguments().getString(ARGUMENT_GAME_SCREENSHOT_PATH))
				.noFade()
				.noPlaceholder()
				.into(imageGameScreen);

		circleBanner.setImageResource(R.drawable.no_banner);

		builder.setView(contents);
		return builder.create();
	}
}
