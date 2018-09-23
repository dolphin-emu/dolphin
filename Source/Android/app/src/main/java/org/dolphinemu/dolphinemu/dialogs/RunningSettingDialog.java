package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.RadioButton;
import android.widget.SeekBar;
import android.widget.TextView;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;

import java.util.ArrayList;

public class RunningSettingDialog extends DialogFragment
{

  public class SettingsItem
  {
    // gfx
    public static final int SETTING_SHOW_FPS = 0;
    public static final int SETTING_EFB_TEXTURE = 1;
    public static final int SETTING_IGNORE_FORMAT = 2;
    public static final int SETTING_ARBITRARY_MIPMAP_DETECTION = 3;
    // core
    public static final int SETTING_SYNC_ON_SKIP_IDLE = 4;
    public static final int SETTING_OVERCLOCK_ENABLE = 5;
    public static final int SETTING_OVERCLOCK_PERCENT = 6;
    public static final int SETTING_JIT_FOLLOW_THRESHOLD = 7;

    // view type
    public static final int TYPE_CHECKBOX = 0;
    public static final int TYPE_RADIO_BUTTON = 1;
    public static final int TYPE_SEEK_BAR = 2;

    private int mSetting;
    private int mNameId;
    private int mType;
    private int mValue;

    public SettingsItem(int setting, int nameId, int type, int value)
    {
      mSetting = setting;
      mNameId = nameId;
      mType = type;
      mValue = value;
    }

    public int getType()
    {
      return mType;
    }

    public int getSetting()
    {
      return mSetting;
    }

    public int getNameId()
    {
      return mNameId;
    }

    public int getValue()
    {
      return mValue;
    }

    public void setValue(int value)
    {
      mValue = value;
    }
  }

  public abstract class SettingViewHolder extends RecyclerView.ViewHolder
          implements View.OnClickListener
  {
    public SettingViewHolder(View itemView)
    {
      super(itemView);
      itemView.setOnClickListener(this);
      findViews(itemView);
    }

    protected abstract void findViews(View root);

    public abstract void bind(SettingsItem item);

    public abstract void onClick(View clicked);
  }


  public final class CheckBoxSettingViewHolder extends SettingViewHolder
  {
    SettingsItem mItem;
    private TextView mTextSettingName;
    private CheckBox mCheckbox;

    public CheckBoxSettingViewHolder(View itemView)
    {
      super(itemView);
    }

    @Override
    protected void findViews(View root)
    {
      mTextSettingName = root.findViewById(R.id.text_setting_name);
      mCheckbox = root.findViewById(R.id.checkbox);
    }

    @Override
    public void bind(SettingsItem item)
    {
      mItem = item;
      mTextSettingName.setText(item.getNameId());
      mCheckbox.setChecked(mItem.getValue() > 0);
    }

    @Override
    public void onClick(View clicked)
    {
      mCheckbox.toggle();
      mItem.setValue(mCheckbox.isChecked() ? 1 : 0);
    }
  }

  public final class RadioButtonSettingViewHolder extends SettingViewHolder
  {
    SettingsItem mItem;
    private TextView mTextSettingName;
    private RadioButton mRadioButton;

    public RadioButtonSettingViewHolder(View itemView)
    {
      super(itemView);
    }

    @Override
    protected void findViews(View root)
    {
      mTextSettingName = root.findViewById(R.id.text_setting_name);
      mRadioButton = root.findViewById(R.id.radiobutton);
    }

    @Override
    public void bind(SettingsItem item)
    {
      mItem = item;
      mTextSettingName.setText(item.getNameId());
      mRadioButton.setChecked(mItem.getValue() > 0);
    }

    @Override
    public void onClick(View clicked)
    {
      mRadioButton.toggle();
    }
  }

  public final class SeekBarSettingViewHolder extends SettingViewHolder
  {
    SettingsItem mItem;
    private TextView mTextSettingName;
    private TextView mTextSettingValue;
    private SeekBar mSeekBar;

    public SeekBarSettingViewHolder(View itemView)
    {
      super(itemView);
    }

    @Override
    protected void findViews(View root)
    {
      mTextSettingName = root.findViewById(R.id.text_setting_name);
      mTextSettingValue = root.findViewById(R.id.text_setting_value);
      mSeekBar = root.findViewById(R.id.seekbar);
    }

    @Override
    public void bind(SettingsItem item)
    {
      mItem = item;
      mTextSettingName.setText(item.getNameId());
      if (mItem.getSetting() == SettingsItem.SETTING_OVERCLOCK_PERCENT)
      {
        mSeekBar.setMax(300);
      }
      else
      {
        mSeekBar.setMax(10);
      }
      mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener()
      {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean b)
        {
          if (mItem.getSetting() == SettingsItem.SETTING_OVERCLOCK_PERCENT)
          {
            mTextSettingValue.setText(progress + "%");
          }
          else
          {
            mTextSettingValue.setText(String.valueOf(progress));
          }
          mItem.setValue(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar)
        {

        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar)
        {

        }
      });
      mSeekBar.setProgress(item.getValue());
    }

    @Override
    public void onClick(View clicked)
    {

    }
  }

  public class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
  {
    private int[] mRunningSettings;
    private ArrayList<SettingsItem> mSettings;

    public SettingsAdapter()
    {
      int i = 0;
      mRunningSettings = NativeLibrary.getRunningSettings();
      mSettings = new ArrayList<>();

      // gfx
      mSettings.add(new SettingsItem(SettingsItem.SETTING_SHOW_FPS, R.string.show_fps,
              SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_EFB_TEXTURE, R.string.efb_copy_method,
              SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_IGNORE_FORMAT,
              R.string.ignore_format_changes, SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_ARBITRARY_MIPMAP_DETECTION,
              R.string.arbitrary_mipmap_detection, SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));

      // core
      mSettings.add(new SettingsItem(SettingsItem.SETTING_SYNC_ON_SKIP_IDLE,
              R.string.sync_on_skip_idle, SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_OVERCLOCK_ENABLE,
              R.string.overclock_enable, SettingsItem.TYPE_CHECKBOX, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_OVERCLOCK_PERCENT,
              R.string.overclock_title, SettingsItem.TYPE_SEEK_BAR, mRunningSettings[i++]));
      mSettings.add(new SettingsItem(SettingsItem.SETTING_JIT_FOLLOW_THRESHOLD,
              R.string.jit_follow_branch, SettingsItem.TYPE_SEEK_BAR, mRunningSettings[i++]));
    }

    @Override
    public SettingViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
    {
      View itemView = null;
      LayoutInflater inflater = LayoutInflater.from(parent.getContext());
      switch (viewType)
      {
        case SettingsItem.TYPE_CHECKBOX:
          itemView = inflater.inflate(R.layout.list_item_running_checkbox, parent, false);
          return new CheckBoxSettingViewHolder(itemView);
        case SettingsItem.TYPE_RADIO_BUTTON:
          itemView = inflater.inflate(R.layout.list_item_running_radiobutton, parent, false);
          return new RadioButtonSettingViewHolder(itemView);
        case SettingsItem.TYPE_SEEK_BAR:
          itemView = inflater.inflate(R.layout.list_item_running_seekbar, parent, false);
          return new SeekBarSettingViewHolder(itemView);
      }
      return null;
    }

    @Override
    public int getItemCount()
    {
      return mSettings.size();
    }

    @Override
    public int getItemViewType(int position)
    {
      return mSettings.get(position).getType();
    }

    @Override
    public void onBindViewHolder(@NonNull SettingViewHolder holder, int position)
    {
      holder.bind(mSettings.get(position));
    }

    public void saveSettings()
    {
      boolean isChanged = false;
      int[] newSettings = new int[mSettings.size()];
      for (int i = 0; i < mSettings.size(); ++i)
      {
        newSettings[i] = mSettings.get(i).getValue();
        if (newSettings[i] != mRunningSettings[i])
        {
          isChanged = true;
        }
      }
      if (isChanged)
      {
        NativeLibrary.setRunningSettings(newSettings);
      }
    }
  }

  public static RunningSettingDialog newInstance()
  {
    return new RunningSettingDialog();
  }

  private SettingsAdapter mAdapter;

  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater()
            .inflate(R.layout.dialog_running_settings, null);

    int columns = 1;
    Drawable lineDivider = getContext().getDrawable(R.drawable.line_divider);
    RecyclerView recyclerView = contents.findViewById(R.id.list_settings);
    RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getContext(), columns);
    recyclerView.setLayoutManager(layoutManager);
    mAdapter = new SettingsAdapter();
    recyclerView.setAdapter(mAdapter);
    recyclerView.addItemDecoration(new DividerItemDecoration(lineDivider));
    builder.setView(contents);
    return builder.create();
  }

  @Override
  public void onDismiss(DialogInterface dialog)
  {
    super.onDismiss(dialog);
    mAdapter.saveSettings();
  }

}
