// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui;

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.provider.DocumentsContract;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.elevation.ElevationOverlayProvider;
import com.google.android.material.slider.Slider;
import com.google.android.material.textfield.TextInputEditText;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.databinding.DialogInputStringBinding;
import org.dolphinemu.dolphinemu.databinding.DialogSliderBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemHeaderBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemSettingBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemSettingCheckboxBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding;
import org.dolphinemu.dolphinemu.dialogs.MotionAlertDialog;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.FilePicker;
import org.dolphinemu.dolphinemu.features.settings.model.view.FloatSliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.IntSliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.RumbleBindingSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SingleChoiceSettingDynamicDescriptions;
import org.dolphinemu.dolphinemu.features.settings.model.view.SliderSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.InputStringSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.features.settings.model.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.CheckBoxSettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.FilePickerViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.HeaderHyperLinkViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.HeaderViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.InputBindingSettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.InputStringSettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.RumbleBindingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.RunRunnableViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SettingViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SingleChoiceViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SliderViewHolder;
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.SubmenuViewHolder;
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization;
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.PermissionsHandler;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;

public final class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
        implements DialogInterface.OnClickListener, Slider.OnChangeListener
{
  private final SettingsFragmentView mView;
  private final Context mContext;
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

  @NonNull
  @Override
  public SettingViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    switch (viewType)
    {
      case SettingsItem.TYPE_HEADER:
        return new HeaderViewHolder(ListItemHeaderBinding.inflate(inflater), this);

      case SettingsItem.TYPE_CHECKBOX:
        return new CheckBoxSettingViewHolder(ListItemSettingCheckboxBinding.inflate(inflater),
                this);

      case SettingsItem.TYPE_STRING_SINGLE_CHOICE:
      case SettingsItem.TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS:
      case SettingsItem.TYPE_SINGLE_CHOICE:
        return new SingleChoiceViewHolder(ListItemSettingBinding.inflate(inflater), this);

      case SettingsItem.TYPE_SLIDER:
        return new SliderViewHolder(ListItemSettingBinding.inflate(inflater), this, mContext);

      case SettingsItem.TYPE_SUBMENU:
        return new SubmenuViewHolder(ListItemSubmenuBinding.inflate(inflater), this);

      case SettingsItem.TYPE_INPUT_BINDING:
        return new InputBindingSettingViewHolder(ListItemSettingBinding.inflate(inflater), this,
                mContext);

      case SettingsItem.TYPE_RUMBLE_BINDING:
        return new RumbleBindingViewHolder(ListItemSettingBinding.inflate(inflater), this,
                mContext);

      case SettingsItem.TYPE_FILE_PICKER:
        return new FilePickerViewHolder(ListItemSettingBinding.inflate(inflater), this);

      case SettingsItem.TYPE_RUN_RUNNABLE:
        return new RunRunnableViewHolder(ListItemSettingBinding.inflate(inflater), this, mContext);

      case SettingsItem.TYPE_STRING:
        return new InputStringSettingViewHolder(ListItemSettingBinding.inflate(inflater), this);

      case SettingsItem.TYPE_HYPERLINK_HEADER:
        return new HeaderHyperLinkViewHolder(ListItemHeaderBinding.inflate(inflater), this);

      default:
        throw new IllegalArgumentException("Invalid view type: " + viewType);
    }
  }

  @Override
  public void onBindViewHolder(@NonNull SettingViewHolder holder, int position)
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

  public Settings getSettings()
  {
    return mView.getSettings();
  }

  public void setSettings(ArrayList<SettingsItem> settings)
  {
    mSettings = settings;
    notifyDataSetChanged();
  }

  public void clearSetting(SettingsItem item, int position)
  {
    item.clear(getSettings());
    notifyItemChanged(position);

    mView.onSettingChanged();
  }

  public void notifyAllSettingsChanged()
  {
    notifyItemRangeChanged(0, getItemCount());

    mView.onSettingChanged();
  }

  public void onBooleanClick(CheckBoxSetting item, int position, boolean checked)
  {
    item.setChecked(getSettings(), checked);
    notifyItemChanged(position);

    mView.onSettingChanged();
  }

  public void onInputStringClick(InputStringSetting item, int position)
  {
    LayoutInflater inflater = LayoutInflater.from(mContext);

    DialogInputStringBinding binding = DialogInputStringBinding.inflate(inflater);
    TextInputEditText input = binding.input;
    input.setText(item.getSelectedValue(getSettings()));

    mDialog = new MaterialAlertDialogBuilder(mView.getActivity())
            .setView(binding.getRoot())
            .setMessage(item.getDescription())
            .setPositiveButton(R.string.ok, (dialogInterface, i) ->
            {
              String editTextInput = input.getText().toString();

              if (!item.getSelectedValue(mView.getSettings()).equals(editTextInput))
              {
                notifyItemChanged(position);
                mView.onSettingChanged();
              }

              item.setSelectedValue(mView.getSettings(), editTextInput);
            })
            .setNegativeButton(R.string.cancel, null)
            .show();
  }

  public void onSingleChoiceClick(SingleChoiceSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    int value = getSelectionForSingleChoiceValue(item);

    mDialog = new MaterialAlertDialogBuilder(mView.getActivity())
            .setTitle(item.getName())
            .setSingleChoiceItems(item.getChoicesId(), value, this)
            .show();
  }

  public void onStringSingleChoiceClick(StringSingleChoiceSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    mDialog = new MaterialAlertDialogBuilder(mView.getActivity())
            .setTitle(item.getName())
            .setSingleChoiceItems(item.getChoices(), item.getSelectedValueIndex(getSettings()),
                    this)
            .show();
  }

  public void onSingleChoiceDynamicDescriptionsClick(SingleChoiceSettingDynamicDescriptions item,
          int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    int value = getSelectionForSingleChoiceDynamicDescriptionsValue(item);

    mDialog = new MaterialAlertDialogBuilder(mView.getActivity())
            .setTitle(item.getName())
            .setSingleChoiceItems(item.getChoicesId(), value, this)
            .show();
  }

  public void onSliderClick(SliderSetting item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;
    mSeekbarProgress = item.getSelectedValue(getSettings());

    LayoutInflater inflater = LayoutInflater.from(mView.getActivity());
    DialogSliderBinding binding = DialogSliderBinding.inflate(inflater);

    mTextSliderValue = binding.textValue;
    mTextSliderValue.setText(String.valueOf(mSeekbarProgress));

    binding.textUnits.setText(item.getUnits());

    Slider slider = binding.slider;
    slider.setValueFrom(item.getMin());
    slider.setValueTo(item.getMax());
    slider.setValue(mSeekbarProgress);
    slider.setStepSize(1);
    slider.addOnChangeListener(this);

    mDialog = new MaterialAlertDialogBuilder(mView.getActivity())
            .setTitle(item.getName())
            .setView(binding.getRoot())
            .setPositiveButton(R.string.ok, this)
            .show();
  }

  public void onSubmenuClick(SubmenuSetting item)
  {
    mView.loadSubMenu(item.getMenuKey());
  }

  public void onInputBindingClick(final InputBindingSetting item, final int position)
  {
    final MotionAlertDialog dialog = new MotionAlertDialog(mContext, item, this);

    Drawable background = ContextCompat.getDrawable(mContext, R.drawable.dialog_round);
    @ColorInt int color = new ElevationOverlayProvider(dialog.getContext()).compositeOverlay(
            MaterialColors.getColor(dialog.getWindow().getDecorView(), R.attr.colorSurface),
            dialog.getWindow().getDecorView().getElevation());
    background.setColorFilter(color, PorterDuff.Mode.SRC_ATOP);
    dialog.getWindow().setBackgroundDrawable(background);

    dialog.setTitle(R.string.input_binding);
    dialog.setMessage(String.format(mContext.getString(
                    item instanceof RumbleBindingSetting ?
                            R.string.input_rumble_description : R.string.input_binding_description),
            item.getName()));
    dialog.setButton(AlertDialog.BUTTON_NEGATIVE, mContext.getString(R.string.cancel), this);
    dialog.setButton(AlertDialog.BUTTON_NEUTRAL, mContext.getString(R.string.clear),
            (dialogInterface, i) -> item.clearValue(getSettings()));
    dialog.setOnDismissListener(dialog1 ->
    {
      notifyItemChanged(position);
      mView.onSettingChanged();
    });
    dialog.setCanceledOnTouchOutside(false);
    dialog.show();
  }

  public void onFilePickerDirectoryClick(SettingsItem item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;

    if (!PermissionsHandler.isExternalStorageLegacy())
    {
      new MaterialAlertDialogBuilder(mContext)
              .setMessage(R.string.path_not_changeable_scoped_storage)
              .setPositiveButton(R.string.ok, (dialog, i) -> dialog.dismiss())
              .show();
    }
    else
    {
      FileBrowserHelper.openDirectoryPicker(mView.getActivity(), FileBrowserHelper.GAME_EXTENSIONS);
    }
  }

  public void onFilePickerFileClick(SettingsItem item, int position)
  {
    mClickedItem = item;
    mClickedPosition = position;
    FilePicker filePicker = (FilePicker) item;

    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("*/*");

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
    {
      intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
              filePicker.getSelectedValue(mView.getSettings()));
    }

    mView.getActivity().startActivityForResult(intent, filePicker.getRequestType());
  }

  public void onFilePickerConfirmation(String selectedFile)
  {
    FilePicker filePicker = (FilePicker) mClickedItem;

    if (!filePicker.getSelectedValue(mView.getSettings()).equals(selectedFile))
    {
      notifyItemChanged(mClickedPosition);
      mView.onSettingChanged();
    }

    filePicker.setSelectedValue(mView.getSettings(), selectedFile);

    mClickedItem = null;
  }

  public static void clearLog()
  {
    // Don't delete the log in case it is being monitored by another app.
    File log = new File(DirectoryInitialization.getUserDirectory() + "/Logs/dolphin.log");

    try
    {
      RandomAccessFile raf = new RandomAccessFile(log, "rw");
      raf.setLength(0);
    }
    catch (IOException e)
    {
      Log.error("[SettingsAdapter] Failed to clear log file: " + e.getMessage());
    }
  }

  private void handleMenuTag(MenuTag menuTag, int value)
  {
    if (menuTag != null)
    {
      if (menuTag.isSerialPort1Menu())
      {
        mView.onSerialPort1SettingChanged(menuTag, value);
      }

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
  }

  @Override
  public void onClick(DialogInterface dialog, int which)
  {
    if (mClickedItem instanceof SingleChoiceSetting)
    {
      SingleChoiceSetting scSetting = (SingleChoiceSetting) mClickedItem;

      int value = getValueForSingleChoiceSelection(scSetting, which);
      if (scSetting.getSelectedValue(getSettings()) != value)
        mView.onSettingChanged();

      handleMenuTag(scSetting.getMenuTag(), value);

      scSetting.setSelectedValue(getSettings(), value);

      closeDialog();
    }
    else if (mClickedItem instanceof SingleChoiceSettingDynamicDescriptions)
    {
      SingleChoiceSettingDynamicDescriptions scSetting =
              (SingleChoiceSettingDynamicDescriptions) mClickedItem;

      int value = getValueForSingleChoiceDynamicDescriptionsSelection(scSetting, which);
      if (scSetting.getSelectedValue(getSettings()) != value)
        mView.onSettingChanged();

      scSetting.setSelectedValue(getSettings(), value);

      closeDialog();
    }
    else if (mClickedItem instanceof StringSingleChoiceSetting)
    {
      StringSingleChoiceSetting scSetting = (StringSingleChoiceSetting) mClickedItem;
      String value = scSetting.getValueAt(which);
      if (!scSetting.getSelectedValue(getSettings()).equals(value))
        mView.onSettingChanged();

      handleMenuTag(scSetting.getMenuTag(), which);

      scSetting.setSelectedValue(getSettings(), value);

      closeDialog();
    }
    else if (mClickedItem instanceof IntSliderSetting)
    {
      IntSliderSetting sliderSetting = (IntSliderSetting) mClickedItem;
      if (sliderSetting.getSelectedValue(getSettings()) != mSeekbarProgress)
        mView.onSettingChanged();

      sliderSetting.setSelectedValue(getSettings(), mSeekbarProgress);

      closeDialog();
    }
    else if (mClickedItem instanceof FloatSliderSetting)
    {
      FloatSliderSetting sliderSetting = (FloatSliderSetting) mClickedItem;
      if (sliderSetting.getSelectedValue(getSettings()) != mSeekbarProgress)
        mView.onSettingChanged();

      sliderSetting.setSelectedValue(getSettings(), mSeekbarProgress);

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
  public void onValueChange(@NonNull Slider slider, float progress, boolean fromUser)
  {
    mSeekbarProgress = (int) progress;
    mTextSliderValue.setText(String.valueOf(mSeekbarProgress));
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
    int value = item.getSelectedValue(getSettings());
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
    int value = item.getSelectedValue(getSettings());
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
}
