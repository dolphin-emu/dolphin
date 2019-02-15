package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.v4.app.DialogFragment;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;

import de.hdodenhof.circleimageview.CircleImageView;

public final class GameDetailsDialog extends DialogFragment
{
  private static final String ARG_GAME_PATH = "game_path";

  public static GameDetailsDialog newInstance(String gamePath)
  {
    GameDetailsDialog fragment = new GameDetailsDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_GAME_PATH, gamePath);
    fragment.setArguments(arguments);

    return fragment;
  }

  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    GameFile gameFile = GameFileCacheService.addOrGet(getArguments().getString(ARG_GAME_PATH));

    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater()
            .inflate(R.layout.dialog_game_details, null);

    final ImageView imageGameScreen = contents.findViewById(R.id.image_game_screen);
    CircleImageView circleBanner = contents.findViewById(R.id.circle_banner);

    TextView textTitle = contents.findViewById(R.id.text_game_title);
    TextView textDescription = contents.findViewById(R.id.text_description);

    TextView textCountry = contents.findViewById(R.id.text_country);
    TextView textCompany = contents.findViewById(R.id.text_company);

    FloatingActionButton buttonLaunch = contents.findViewById(R.id.button_launch);

    String country = getResources().getStringArray(R.array.countryNames)[gameFile.getCountry()];

    textTitle.setText(gameFile.getTitle());
    textDescription.setText(gameFile.getDescription());
    textCountry.setText(country);
    textCompany.setText(gameFile.getCompany());

    buttonLaunch.setOnClickListener(view ->
    {
      // Start the emulation activity and send the path of the clicked ROM to it.
      EmulationActivity.launch(getActivity(), gameFile);
    });

    // Fill in the view contents.
    Picasso.with(imageGameScreen.getContext())
            .load(getArguments().getString(gameFile.getScreenshotPath()))
            .fit()
            .centerCrop()
            .noFade()
            .noPlaceholder()
            .into(imageGameScreen);

    circleBanner.setImageResource(R.drawable.no_banner);

    builder.setView(contents);
    return builder.create();
  }
}
