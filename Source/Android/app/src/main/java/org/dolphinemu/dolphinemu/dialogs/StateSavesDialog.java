package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;

public class StateSavesDialog extends DialogFragment
{
  public class StateSaveModel
  {
    private int mIndex;
    private String mFilename;
    private long mLastModified;

    public StateSaveModel(int index, String filename, long lastModified)
    {
      mIndex = index;
      mFilename = filename;
      mLastModified = lastModified;
    }

    public int getIndex()
    {
      return mIndex;
    }

    public String getName()
    {
      int idx = mFilename.lastIndexOf(File.separatorChar);
      if(idx != -1 && idx < mFilename.length())
        return mFilename.substring(idx + 1);
      return "";
    }

    public String getFilename()
    {
      return mFilename;
    }

    public long getLastModified()
    {
      return mLastModified;
    }
  }

  public class StateSaveViewHolder extends RecyclerView.ViewHolder
  {
    private DialogFragment mDialog;
    private TextView mName;
    private TextView mDate;
    private Button mBtnLoad;
    private Button mBtnSave;
    private Button mBtnDelete;

    public StateSaveViewHolder(DialogFragment dialog, View itemView)
    {
      super(itemView);

      mDialog = dialog;
      mName = itemView.findViewById(R.id.state_name);
      mDate = itemView.findViewById(R.id.state_time);
      mBtnLoad = itemView.findViewById(R.id.button_load_state);
      mBtnSave = itemView.findViewById(R.id.button_save_state);
      mBtnDelete = itemView.findViewById(R.id.button_delete);
    }

    public void bind(StateSaveModel item)
    {
      long lastModified = item.getLastModified();
      if(lastModified > 0)
      {
        mName.setText(item.getName());
        mDate.setText(SimpleDateFormat.getDateTimeInstance().format(new Date(lastModified)));
        mBtnDelete.setVisibility(View.VISIBLE);
      }
      else
      {
        mName.setText("");
        mDate.setText("");
        mBtnDelete.setVisibility(View.INVISIBLE);
      }
      mBtnLoad.setEnabled(!item.getName().isEmpty());
      mBtnLoad.setOnClickListener(view ->
      {
        NativeLibrary.LoadState(item.getIndex());
        mDialog.dismiss();
      });
      mBtnSave.setOnClickListener(view ->
      {
        NativeLibrary.SaveState(item.getIndex(), false);
        mDialog.dismiss();
      });
      mBtnDelete.setOnClickListener(view ->
      {
        File file = new File(item.getFilename());
        if(file.delete())
        {
          mName.setText("");
          mDate.setText("");
          mBtnDelete.setVisibility(View.INVISIBLE);
        }
      });
    }
  }

  public class StateSavesAdapter extends RecyclerView.Adapter<StateSaveViewHolder>
  {
    private static final int NUM_STATES = 10;
    private DialogFragment mDialog;
    private ArrayList<StateSaveModel> mStateSaves;

    public StateSavesAdapter(DialogFragment dialog, String gameId)
    {
      final String statePath = DirectoryInitialization.getUserDirectory() + "/StateSaves/";
      ArrayList<Integer> indices = new ArrayList<Integer>();
      mDialog = dialog;
      mStateSaves = new ArrayList<>();
      for (int i = 0; i < NUM_STATES; ++i)
      {
        String filename = String.format("%s%s.s%02d", statePath, gameId, i);
        File stateFile = new File(filename);
        if (stateFile.exists())
        {
          mStateSaves.add(new StateSaveModel(i, filename, stateFile.lastModified()));
        }
        else
        {
          indices.add(i);
        }
      }

      for(Integer idx : indices)
      {
        mStateSaves.add(new StateSaveModel(idx, "", 0));
      }
    }

    @NonNull
    @Override
    public StateSaveViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
    {
      LayoutInflater inflater = LayoutInflater.from(parent.getContext());
      View itemView = inflater.inflate(R.layout.list_item_statesave, parent, false);
      return new StateSaveViewHolder(mDialog, itemView);
    }

    @Override
    public int getItemCount()
    {
      return mStateSaves.size();
    }

    @Override
    public int getItemViewType(int position)
    {
      return 0;
    }

    @Override
    public void onBindViewHolder(@NonNull StateSaveViewHolder holder, int position)
    {
      holder.bind(mStateSaves.get(position));
    }
  }

  public static StateSavesDialog newInstance(String gameId)
  {
    StateSavesDialog fragment = new StateSavesDialog();
    Bundle arguments = new Bundle();
    arguments.putString(ARG_GAME_ID, gameId);
    fragment.setArguments(arguments);
    return fragment;
  }

  private static final String ARG_GAME_ID = "game_id";
  private StateSavesAdapter mAdapter;

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater()
      .inflate(R.layout.dialog_running_settings, null);

    TextView textTitle = contents.findViewById(R.id.text_title);
    textTitle.setText(R.string.state_saves);

    int columns = 1;
    Drawable lineDivider = getContext().getDrawable(R.drawable.line_divider);
    RecyclerView recyclerView = contents.findViewById(R.id.list_settings);
    RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getContext(), columns);
    recyclerView.setLayoutManager(layoutManager);
    mAdapter = new StateSavesAdapter(this, getArguments().getString(ARG_GAME_ID));
    recyclerView.setAdapter(mAdapter);
    recyclerView.addItemDecoration(new DividerItemDecoration(lineDivider));
    builder.setView(contents);
    return builder.create();
  }
}
