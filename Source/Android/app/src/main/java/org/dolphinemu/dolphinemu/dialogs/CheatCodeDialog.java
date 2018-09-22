package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheService;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;

public class CheatCodeDialog extends DialogFragment
{
  public static final int CODE_TYPE_AR = 0;
  public static final int CODE_TYPE_GECKO = 1;

  private static final String ARG_GAME_PATH = "game_path";

  public static CheatCodeDialog newInstance(String gamePath)
  {
    CheatCodeDialog fragment = new CheatCodeDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_GAME_PATH, gamePath);
    fragment.setArguments(arguments);

    return fragment;
  }

  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final GameFile gameFile =
            GameFileCacheService.addOrGet(getArguments().getString(ARG_GAME_PATH));
    final String gameId = gameFile.getGameId();
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater()
            .inflate(R.layout.dialog_cheat_code_editor, null);

    TextView textGameId = contents.findViewById(R.id.game_id);
    textGameId.setText(gameId);

    EditText editCodeContent = contents.findViewById(R.id.code_content);
    editCodeContent.setHorizontallyScrolling(true);
    setGameSettings(gameId, editCodeContent);

    Button buttonAdd = contents.findViewById(R.id.button_add_code);
    buttonAdd.setOnClickListener(view ->
    {
      String content = editCodeContent.getText().toString();
      AcceptCheatCode(gameId, content);
      this.dismiss();
    });

    builder.setView(contents);
    return builder.create();
  }

  private void setGameSettings(String gameId, EditText editCode)
  {
    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
    int index1 = -1;
    int index2 = -1;
    int count = 0;
    String target1 = "ActionReplay";
    String target2 = "Gecko";
    StringBuilder sb = new StringBuilder();
    try
    {
      BufferedReader reader = new BufferedReader(new FileReader(filename));
      String line = reader.readLine();
      while (line != null)
      {
        //
        int index = line.indexOf(target1);
        if (index > 0)
        {
          index1 += count + index;
        }
        //
        index = line.indexOf(target2);
        if (index > 0)
        {
          index2 += count + index;
        }
        //
        count += line.length() + 1;
        sb.append(line);
        sb.append(System.lineSeparator());
        line = reader.readLine();
      }
    }
    catch (Exception e)
    {
      // ignore
    }

    if (index1 == -1 && index2 == -1)
    {
      sb.append(System.lineSeparator());
      sb.append("[ActionReplay]");
      sb.append(System.lineSeparator());
      sb.append(System.lineSeparator());
      sb.append("[ActionReplay_Enabled]");
      sb.append(System.lineSeparator());
      index1 += count + 2;

      sb.append(System.lineSeparator());
      sb.append("[Gecko]");
      sb.append(System.lineSeparator());
      sb.append(System.lineSeparator());
      sb.append("[Gecko_Enabled]");
      sb.append(System.lineSeparator());
      index2 += count + 2;
    }

    int cursorPos = 0;
    if (index1 != -1)
    {
      cursorPos = index1 + target1.length() + 3;
    }
    else if (index2 != -1)
    {
      cursorPos = index2 + target2.length() + 3;
    }
    editCode.setText(sb.toString());
    editCode.setSelection(cursorPos);
  }

  private void AcceptCheatCode(String gameId, String content)
  {
    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
    //String[] lines = content.split(System.lineSeparator());
    boolean saved = false;
    try
    {
      BufferedWriter writer = new BufferedWriter(new FileWriter(filename));
      writer.write(content);
      writer.close();
      saved = true;
    }
    catch (Exception e)
    {
      // ignore
    }
    Toast.makeText(getContext(), saved ? R.string.toast_save_code_ok : R.string.toast_save_code_no,
            Toast.LENGTH_SHORT).show();
  }
}
