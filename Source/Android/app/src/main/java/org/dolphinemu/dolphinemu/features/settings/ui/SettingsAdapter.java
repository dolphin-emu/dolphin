package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.SeekBar;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.dialogs.MotionAlertDialog;
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting;
import org.dolphinemu.dolphinemu.features.settings.model.FloatSetting;
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.RumbleBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.CheckBoxSettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.HeaderViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.InputBindingSettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.RumbleBindingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SingleChoiceViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SliderViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SubmenuViewHolder;
import org.dolphinemu.dolphinemu.features.settings.utils.SettingsFile;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.ArrayList;

public final class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
        implements DialogInterface.OnClickListener, SeekBar.OnSeekBarChangeListener
{
  private SettingsFragmentView mView;
  private Context mContext;
  private ArrayList<SettingsItem> mSettings;

  private SettingsItem mClickedItem;
  private int mClickedPosition;
  private int mSeekbarProgress;

  private AlertDialog mDialog;
  private TextView mTextSliderValue;

  public SettingsAdapter(SettingsFragmentView view, Context context)
  {
    mView = view;
    mContext = context;
    mClickedPosition = -1;
  }

  @Override
  public SettingViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    View view;
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());

    switch (viewType)
    {
      case SettingsItem.TYPE_HEADER:
        view = inflater.inflate(R.layout.list_item_settings_header, parent, false);
        return new HeaderViewHolder(view, this);

      case SettingsItem.TYPE_CHECKBOX:
        view = inflater.inflate(R.layout.list_item_setting_checkbox, parent, false);
        return new CheckBoxSettingViewHolder(view, this);

      case SettingsItem.TYPE_STRING_SINGLE_CHOICE:
      case SettingsItem.TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS:
      case SettingsItem.TYPE_SINGLE_CHOICE:
        view = inflater.inflate(R.layout.list_item_setting, parent, false);
        return new SingleChoiceViewHolder(view, this);

      case SettingsItem.TYPE_SLIDER:
        view = inflater.inflate(R.layout.list_item_setting, parent, false);
        return new SliderViewHolder(view, this);

      case SettingsItem.TYPE_SUBMENU:
        view = inflater.inflate(R.layout.list_item_setting, parent, false);
        return new SubmenuViewHolder(view, this);

      case SettingsItem.TYPE_INPUT_BINDING:
        view = inflater.inflate(R.layout.list_item_setting, parent, false);
        return new InputBindingSettingViewHolder(view, this, mContext);

      case SettingsItem.TYPE_RUMBLE_BINDING:
        view = inflater.inflate(R.layout.list_item_setting, parent, false);
        return new RumbleBindingViewHolder(view, this, mContext);

      default:
        Log.error("[SettingsAdapter] Invalid view type: " + viewType);
        return null;
    }
  }

  @Override
  public void onBindViewHolder(SettingViewHolder holder, int position)
  {
    holder.bind(getItem(position));
  }

  private SettingsItem getItem(int position)
  {
    return mSettings.get(position);
  }

  @Override
  public int getItemCount()
  {
    if (mSettings != null)
    {
      return mSettings.size();
    }
    else
    {
      return 0;
    }
  }

  @Override
  public int getItemViewType(int position)
  {
    return getItem(position).getType();
  }

  public void setSettings(ArrayList<SettingsItem> settings)
  {
    mSettings = settings;
    notifyDataSetChanged();
  }

  public void onBooleanClick(CheckBoxSetting item, int position, boolean checked)
  {
    BooleanSetting setting = item.setChecked(checked);
    notifyItemChanged(position);

    if (setting != null)
    {
      mView.putSetting(setting);
    }

    if (item.getKey().equals(SettingsFile.KEY_SKIP_EFB) ||
            item.getKey().equals(SettingsFile.KEY_IGNORE_FORMAT))
    {
      mView.putSetting(new BooleanSetting(item.getKey(), item.getSection(), !checked));
    }

    mView.onSettingChanged();
  }

  public void onSingleChoiceClick(SingleChoiceSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    int value = getSelectionForSingleChoiceValue(item);

    AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

    builder.setTitle(item.getNameId());
    builder.setSingleChoiceItems(item.getChoicesId(), value, this);

    mDialog = builder.show();
  }

  public void onStringSingleChoiceClick(StringSingleChoiceSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

    builder.setTitle(item.getNameId());
    builder.setSingleChoiceItems(item.getChoicesId(), item.getSelectValueIndex(), this);

    mDialog = builder.show();
  }

  public void onSingleChoiceDynamicDescriptionsClick(SingleChoiceSettingDynamicDescriptions item,
          int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    int value = getSelectionForSingleChoiceDynamicDescriptionsValue(item);

    AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

    builder.setTitle(item.getNameId());
    builder.setSingleChoiceItems(item.getChoicesId(), value, this);

    mDialog = builder.show();
  }

  public void onSliderClick(SliderSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;
    mSeekbarProgress = item.getSelectedValue();
    AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

    LayoutInflater inflater = LayoutInflater.from(mView.getActivity());
    View view = inflater.inflate(R.layout.dialog_seekbar, null);

    builder.setTitle(item.getNameId());
    builder.setView(view);
    builder.setPositiveButton(R.string.ok, this);
    mDialog = builder.show();

    mTextSliderValue = (TextView) view.findViewById(R.id.text_value);
    mTextSliderValue.setText(String.valueOf(mSeekbarProgress));

    TextView units = (TextView) view.findViewById(R.id.text_units);
    units.setText(item.getUnits());

    SeekBar seekbar = (SeekBar) view.findViewById(R.id.seekbar);

    seekbar.setMax(item.getMax());
    seekbar.setProgress(mSeekbarProgress);
    seekbar.setKeyProgressIncrement(5);

    seekbar.setOnSeekBarChangeListener(this);
  }

  public void onSubmenuClick(SubmenuSetting item)
  {
    mView.loadSubMenu(item.getMenuKey());
  }

  public void onInputBindingClick(final InputBindingSetting item, final int position)
  {
    final MotionAlertDialog dialog = new MotionAlertDialog(mContext, item);
    dialog.setTitle(R.string.input_binding);
    dialog.setMessage(String.format(mContext.getString(
            item instanceof RumbleBindingSetting ?
                    R.string.input_rumble_description : R.string.input_binding_description),
            mContext.getString(item.getNameId())));
    dialog.setButton(AlertDialog.BUTTON_NEGATIVE, mContext.getString(R.string.cancel), this);
    dialog.setButton(AlertDialog.BUTTON_NEUTRAL, mContext.getString(R.string.clear),
            (dialogInterface, i) ->
            {
              SharedPreferences preferences =
                      PreferenceManager.getDefaultSharedPreferences(mContext);
              item.clearValue();
            });
    dialog.setOnDismissListener(dialog1 ->
    {
      StringSetting setting = new StringSetting(item.getKey(), item.getSection(), item.getValue());
      notifyItemChanged(position);

      if (setting != null)
      {
        mView.putSetting(setting);
      }

      mView.onSettingChanged();
    });
    dialog.setCanceledOnTouchOutside(false);
    dialog.show();
  }

  @Override
  public void onClick(DialogInterface dialog, int which)
  {
    if (mClickedItem instanceof SingleChoiceSetting)
    {
      SingleChoiceSetting scSetting = (SingleChoiceSetting) mClickedItem;

      int value = getValueForSingleChoiceSelection(scSetting, which);
      if (scSetting.getSelectedValue() != value)
        mView.onSettingChanged();

      MenuTag menuTag = scSetting.getMenuTag();
      if (menuTag != null)
      {
        if (menuTag.isGCPadMenu())
        {
          mView.onGcPadSettingChanged(menuTag, value);
        }

        if (menuTag.isWiimoteMenu())
        {
          mView.onWiimoteSettingChanged(menuTag, value);
        }

        if (menuTag.isWiimoteExtensionMenu())
        {
          mView.onExtensionSettingChanged(menuTag, value);
        }
      }

      // Get the backing Setting, which may be null (if for example it was missing from the file)
      IntSetting setting = scSetting.setSelectedValue(value);
      if (setting != null)
      {
        mView.putSetting(setting);
      }
      else
      {
        if (scSetting.getKey().equals(SettingsFile.KEY_VIDEO_BACKEND_INDEX))
        {
          putVideoBackendSetting(which);
        }
        else if (scSetting.getKey().equals(SettingsFile.KEY_WIIMOTE_EXTENSION))
        {
          putExtensionSetting(which, Character.getNumericValue(
                  scSetting.getSection().charAt(scSetting.getSection().length() - 1)), false);
        }
        else if (scSetting.getKey().contains(SettingsFile.KEY_WIIMOTE_EXTENSION) &&
                scSetting.getSection().equals(Settings.SECTION_CONTROLS))
        {
          putExtensionSetting(which, Character
                          .getNumericValue(scSetting.getKey().charAt(scSetting.getKey().length() - 1)),
                  true);
        }
      }

      closeDialog();
    }
    else if (mClickedItem instanceof SingleChoiceSettingDynamicDescriptions)
    {
      SingleChoiceSettingDynamicDescriptions scSetting =
              (SingleChoiceSettingDynamicDescriptions) mClickedItem;

      int value = getValueForSingleChoiceDynamicDescriptionsSelection(scSetting, which);
      if (scSetting.getSelectedValue() != value)
        mView.onSettingChanged();

      // Get the backing Setting, which may be null (if for example it was missing from the file)
      IntSetting setting = scSetting.setSelectedValue(value);
      if (setting != null)
      {
        mView.putSetting(setting);
      }

      closeDialog();
    }
    else if (mClickedItem instanceof StringSingleChoiceSetting)
    {
      StringSingleChoiceSetting scSetting = (StringSingleChoiceSetting) mClickedItem;
      String value = scSetting.getValueAt(which);
      if (!scSetting.getSelectedValue().equals(value))
        mView.onSettingChanged();

      StringSetting setting = scSetting.setSelectedValue(value);
      if (setting != null)
      {
        mView.putSetting(setting);
      }

      closeDialog();
    }
    else if (mClickedItem instanceof SliderSetting)
    {
      SliderSetting sliderSetting = (SliderSetting) mClickedItem;
      if (sliderSetting.getSelectedValue() != mSeekbarProgress)
        mView.onSettingChanged();

      if (sliderSetting.isPercentSetting() || sliderSetting.getSetting() instanceof FloatSetting)
      {
        float value;

        if (sliderSetting.isPercentSetting())
        {
          value = mSeekbarProgress / 100.0f;
        }
        else
        {
          value = (float) mSeekbarProgress;
        }

        FloatSetting setting = sliderSetting.setSelectedValue(value);
        if (setting != null)
        {
          mView.putSetting(setting);
        }
      }
      else
      {
        IntSetting setting = sliderSetting.setSelectedValue(mSeekbarProgress);
        if (setting != null)
        {
          mView.putSetting(setting);
        }
      }

      closeDialog();
    }

    mClickedItem = null;
    mSeekbarProgress = -1;
  }

  public void closeDialog()
  {
    if (mDialog != null)
    {
      if (mClickedPosition != -1)
      {
        notifyItemChanged(mClickedPosition);
        mClickedPosition = -1;
      }
      mDialog.dismiss();
      mDialog = null;
    }
  }

  @Override
  public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser)
  {
    mSeekbarProgress = progress;
    mTextSliderValue.setText(String.valueOf(mSeekbarProgress));
  }

  @Override
  public void onStartTrackingTouch(SeekBar seekBar)
  {
  }

  @Override
  public void onStopTrackingTouch(SeekBar seekBar)
  {
  }

  private int getValueForSingleChoiceSelection(SingleChoiceSetting item, int which)
  {
    int valuesId = item.getValuesId();

    if (valuesId > 0)
    {
      int[] valuesArray = mContext.getResources().getIntArray(valuesId);
      return valuesArray[which];
    }
    else
    {
      return which;
    }
  }

  private int getSelectionForSingleChoiceValue(SingleChoiceSetting item)
  {
    int value = item.getSelectedValue();
    int valuesId = item.getValuesId();

    if (valuesId > 0)
    {
      int[] valuesArray = mContext.getResources().getIntArray(valuesId);
      for (int index = 0; index < valuesArray.length; index++)
      {
        int current = valuesArray[index];
        if (current == value)
        {
          return index;
        }
      }
    }
    else
    {
      return value;
    }

    return -1;
  }

  private int getValueForSingleChoiceDynamicDescriptionsSelection(
          SingleChoiceSettingDynamicDescriptions item, int which)
  {
    int valuesId = item.getValuesId();

    if (valuesId > 0)
    {
      int[] valuesArray = mContext.getResources().getIntArray(valuesId);
      return valuesArray[which];
    }
    else
    {
      return which;
    }
  }

  private int getSelectionForSingleChoiceDynamicDescriptionsValue(
          SingleChoiceSettingDynamicDescriptions item)
  {
    int value = item.getSelectedValue();
    int valuesId = item.getValuesId();

    if (valuesId > 0)
    {
      int[] valuesArray = mContext.getResources().getIntArray(valuesId);
      for (int index = 0; index < valuesArray.length; index++)
      {
        int current = valuesArray[index];
        if (current == value)
        {
          return index;
        }
      }
    }
    else
    {
      return value;
    }

    return -1;
  }

  private void putVideoBackendSetting(int which)
  {
    StringSetting gfxBackend = null;
    switch (which)
    {
      case 0:
        gfxBackend =
                new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, Settings.SECTION_INI_CORE, "OGL");
        break;

      case 1:
        gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, Settings.SECTION_INI_CORE,
                "Vulkan");
        break;

      case 2:
        gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, Settings.SECTION_INI_CORE,
                "Software Renderer");
        break;

      case 3:
        gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, Settings.SECTION_INI_CORE,
                "Null");
        break;
    }

    mView.putSetting(gfxBackend);
  }

  private void putExtensionSetting(int which, int wiimoteNumber, boolean isGame)
  {
    if (!isGame)
    {
      StringSetting extension = new StringSetting(SettingsFile.KEY_WIIMOTE_EXTENSION,
              Settings.SECTION_WIIMOTE + wiimoteNumber,
              mContext.getResources().getStringArray(R.array.wiimoteExtensionsEntries)[which]);
      mView.putSetting(extension);
    }
    else
    {
      StringSetting extension =
              new StringSetting(SettingsFile.KEY_WIIMOTE_EXTENSION + wiimoteNumber,
                      Settings.SECTION_CONTROLS, mContext.getResources()
                      .getStringArray(R.array.wiimoteExtensionsEntries)[which]);
      mView.putSetting(extension);
    }
  }
}
