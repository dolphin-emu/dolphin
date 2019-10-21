package org.dolphinemu.dolphinemu.dialogs;

import android.app.Dialog;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.PicassoUtils;

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

    AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
    ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater()
            .inflate(R.layout.dialog_game_details, null);

    ImageView banner = contents.findViewById(R.id.banner);

    TextView textTitle = contents.findViewById(R.id.text_game_title);
    TextView textDescription = contents.findViewById(R.id.text_description);

    TextView textCountry = contents.findViewById(R.id.text_country);
    TextView textCompany = contents.findViewById(R.id.text_company);
    TextView textGameId = contents.findViewById(R.id.text_game_id);
    TextView textRevision = contents.findViewById(R.id.text_revision);

    String country = getResources().getStringArray(R.array.countryNames)[gameFile.getCountry()];
    String description = gameFile.getDescription();

    textTitle.setText(gameFile.getTitle());
    textDescription.setText(gameFile.getDescription());
    if (description.isEmpty())
    {
      textDescription.setVisibility(View.GONE);
    }
    textCountry.setText(country);
    textCompany.setText(gameFile.getCompany());
    textGameId.setText(gameFile.getGameId());
    textRevision.setText(Integer.toString(gameFile.getRevision()));

    PicassoUtils.loadGameBanner(banner, gameFile);

    builder.setView(contents);
    return builder.create();
  }
}
