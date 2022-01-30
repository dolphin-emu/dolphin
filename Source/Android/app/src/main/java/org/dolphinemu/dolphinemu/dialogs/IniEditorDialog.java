package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;

public class IniEditorDialog extends DialogFragment
{
  private static final String ARG_GAME_ID = "game_id";
  private ViewGroup mViewGroup;

  public static IniEditorDialog newInstance(String gameId)
  {
    IniEditorDialog fragment = new IniEditorDialog();

    Bundle arguments = new Bundle();
    arguments.putString(ARG_GAME_ID, gameId);
    fragment.setArguments(arguments);

    return fragment;
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final String gameId = getArguments().getString(ARG_GAME_ID);
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    mViewGroup = (ViewGroup) getActivity().getLayoutInflater()
      .inflate(R.layout.dialog_ini_editor, null);

    TextView textGameId = mViewGroup.findViewById(R.id.game_id);
    textGameId.setText(gameId);

    EditText editCodeContent = mViewGroup.findViewById(R.id.code_content);
    editCodeContent.setHorizontallyScrolling(true);
    setGameSettings(gameId, editCodeContent);

    Button buttonSave = mViewGroup.findViewById(R.id.button_save_ini_file);
    buttonSave.setOnClickListener(view ->
    {
      String content = editCodeContent.getText().toString();
      saveIniContent(gameId, content);
      this.dismiss();
    });

    builder.setView(mViewGroup);
    return builder.create();
  }

  private void setGameSettings(String gameId, EditText editCode)
  {
    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
    int count = 0;
    String[] targets = {"[ActionReplay]", "[ActionReplay_Enabled]", "[Gecko]", "[Gecko_Enabled]"};
    int[] indices = {-1, -1, -1, -1};

    StringBuilder sb = new StringBuilder();
    try
    {
      BufferedReader reader = new BufferedReader(new FileReader(filename));
      String line = reader.readLine();
      while (line != null)
      {
        for (int i = 0; i < targets.length; ++i)
        {
          int index = line.indexOf(targets[i]);
          if (index != -1)
          {
            indices[i] = count + index;
          }
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
      Log.error(e.getMessage());
      onSaveError();
    }

    int cursorPos = 0;
    for (int i = 0; i < targets.length; ++i)
    {
      if (indices[i] < 0)
      {
        sb.append(System.lineSeparator());
        sb.append(targets[i]);
        sb.append(System.lineSeparator());
        count += targets[i].length() + 2;
        cursorPos = count;
      }
      else if (indices[i] > cursorPos)
      {
        cursorPos = indices[i] + targets[i].length() + 1;
      }
    }

    String content = sb.toString();
    if (cursorPos > content.length())
      cursorPos = content.length();
    editCode.setText(content);
    editCode.setSelection(cursorPos);
  }

  private void saveIniContent(String gameId, String content)
  {
    String filename = DirectoryInitialization.getLocalSettingFile(gameId);
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
      Log.error(e.getMessage());
      onSaveError();
    }

    Toast.makeText(getContext(), saved ? R.string.settings_saved : R.string.error,
      Toast.LENGTH_SHORT).show();
  }

  public void onSaveError()
  {
    TextView textError = mViewGroup.findViewById(R.id.ini_editor_error);
    textError.setText(R.string.error);
  }
}
