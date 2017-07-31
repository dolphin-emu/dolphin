package org.dolphinemu.dolphinemu.ui.settings;

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
import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.FloatSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.StringSetting;
import org.dolphinemu.dolphinemu.model.settings.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.model.settings.view.InputBindingSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.model.settings.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SliderSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.CheckBoxSettingViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.HeaderViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.InputBindingSettingViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SettingViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SingleChoiceViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SliderViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SubmenuViewHolder;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;

public final class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
		implements DialogInterface.OnClickListener, SeekBar.OnSeekBarChangeListener
{
	private SettingsFragmentView mView;
	private Context mContext;
	private ArrayList<SettingsItem> mSettings;

	private SettingsItem mClickedItem;
	private int mSeekbarProgress;

	private AlertDialog mDialog;
	private TextView mTextSliderValue;

	public SettingsAdapter(SettingsFragmentView view, Context context)
	{
		mView = view;
		mContext = context;
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

		if (item.getKey().equals(SettingsFile.KEY_SKIP_EFB) || item.getKey().equals(SettingsFile.KEY_IGNORE_FORMAT))
		{
			mView.putSetting(new BooleanSetting(item.getKey(), item.getSection(), item.getFile(), !checked));
		}

		mView.onSettingChanged();
	}

	public void onSingleChoiceClick(SingleChoiceSetting item)
	{
		mClickedItem = item;

		int value = getSelectionForSingleChoiceValue(item);

		AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

		builder.setTitle(item.getNameId());
		builder.setSingleChoiceItems(item.getChoicesId(), value, this);

		mDialog = builder.show();
	}

	public void onSliderClick(SliderSetting item)
	{
		mClickedItem = item;
		mSeekbarProgress = item.getSelectedValue();
		AlertDialog.Builder builder = new AlertDialog.Builder(mView.getActivity());

		LayoutInflater inflater = LayoutInflater.from(mView.getActivity());
		View view = inflater.inflate(R.layout.dialog_seekbar, null);

		builder.setTitle(item.getNameId());
		builder.setView(view);
		builder.setPositiveButton(R.string.ok, this);
		builder.setNegativeButton(R.string.cancel, this);
		mDialog = builder.show();

		mTextSliderValue = (TextView) view.findViewById(R.id.text_value);
		mTextSliderValue.setText(String.valueOf(mSeekbarProgress));

		TextView units = (TextView) view.findViewById(R.id.text_units);
		units.setText(item.getUnits());

		SeekBar seekbar = (SeekBar) view.findViewById(R.id.seekbar);

		seekbar.setMax(item.getMax());
		seekbar.setProgress(mSeekbarProgress);

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
		dialog.setMessage(String.format(mContext.getString(R.string.input_binding_descrip), mContext.getString(item.getNameId())));
		dialog.setButton(AlertDialog.BUTTON_NEGATIVE, mContext.getString(R.string.cancel), this);
		dialog.setButton(AlertDialog.BUTTON_NEUTRAL, mContext.getString(R.string.clear), new AlertDialog.OnClickListener()
		{
			@Override
			public void onClick(DialogInterface dialogInterface, int i)
			{
				item.setValue("");

				SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(mContext);
				SharedPreferences.Editor editor = sharedPreferences.edit();
				editor.remove(item.getKey());
				editor.apply();
			}
		});
		dialog.setOnDismissListener(new AlertDialog.OnDismissListener()
		{
			@Override
			public void onDismiss(DialogInterface dialog)
			{
				StringSetting setting = new StringSetting(item.getKey(), item.getSection(), item.getFile(), item.getValue());
				notifyItemChanged(position);

				if (setting != null)
				{
					mView.putSetting(setting);
				}

				mView.onSettingChanged();
			}
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

			if (scSetting.getKey().startsWith(SettingsFile.KEY_GCPAD_TYPE))
			{
				mView.onGcPadSettingChanged(scSetting.getKey(), value);
			}

			if (scSetting.getKey().equals(SettingsFile.KEY_WIIMOTE_TYPE))
			{
				mView.onWiimoteSettingChanged(scSetting.getSection(), value);
			}

			if (scSetting.getKey().equals(SettingsFile.KEY_WIIMOTE_EXTENSION))
			{
				mView.onExtensionSettingChanged(scSetting.getKey() + Character.getNumericValue(scSetting.getSection().charAt(scSetting.getSection().length() - 1)), value);
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
				else if (scSetting.getKey().equals(SettingsFile.KEY_XFB_METHOD))
				{
					putXfbSetting(which);
				}
				else if (scSetting.getKey().equals(SettingsFile.KEY_UBERSHADER_MODE))
				{
					putUberShaderModeSetting(which);
				}
				else if (scSetting.getKey().equals(SettingsFile.KEY_WIIMOTE_EXTENSION))
				{
					putExtensionSetting(which, Character.getNumericValue(scSetting.getSection().charAt(scSetting.getSection().length() - 1)));
				}
			}

			closeDialog();
		}
		else if (mClickedItem instanceof SliderSetting)
		{
			SliderSetting sliderSetting = (SliderSetting) mClickedItem;
			if (sliderSetting.getSetting() instanceof FloatSetting)
			{
				float value;

				if (sliderSetting.getKey().equals(SettingsFile.KEY_OVERCLOCK_PERCENT))
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
		}

		mView.onSettingChanged();
		mClickedItem = null;
		mSeekbarProgress = -1;
	}

	public void closeDialog()
	{
		if (mDialog != null)
		{
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

	public void putVideoBackendSetting(int which)
	{
		StringSetting gfxBackend = null;
		switch (which)
		{
			case 0:
				gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, "OGL");
				break;

			case 1:
				gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, "Vulkan");
				break;

			case 2:
				gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, "Software Renderer");
				break;

			case 3:
				gfxBackend = new StringSetting(SettingsFile.KEY_VIDEO_BACKEND, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, "Null");
				break;
		}

		mView.putSetting(gfxBackend);
	}

	public void putXfbSetting(int which)
	{
		BooleanSetting xfbEnable = null;
		BooleanSetting xfbReal = null;

		switch (which)
		{
			case 0:
				xfbEnable = new BooleanSetting(SettingsFile.KEY_XFB, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				xfbReal = new BooleanSetting(SettingsFile.KEY_XFB_REAL, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				break;

			case 1:
				xfbEnable = new BooleanSetting(SettingsFile.KEY_XFB, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, true);
				xfbReal = new BooleanSetting(SettingsFile.KEY_XFB_REAL, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				break;

			case 2:
				xfbEnable = new BooleanSetting(SettingsFile.KEY_XFB, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, true);
				xfbReal = new BooleanSetting(SettingsFile.KEY_XFB_REAL, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, true);
				break;
		}

		mView.putSetting(xfbEnable);
		mView.putSetting(xfbReal);
	}

  public void putUberShaderModeSetting(int which)
  {
		BooleanSetting disableSpecializedShaders = null;
		BooleanSetting backgroundShaderCompilation = null;

		switch (which)
		{
			case 0:
				disableSpecializedShaders = new BooleanSetting(SettingsFile.KEY_DISABLE_SPECIALIZED_SHADERS, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				backgroundShaderCompilation = new BooleanSetting(SettingsFile.KEY_BACKGROUND_SHADER_COMPILING, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				break;

			case 1:
				disableSpecializedShaders = new BooleanSetting(SettingsFile.KEY_DISABLE_SPECIALIZED_SHADERS, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				backgroundShaderCompilation = new BooleanSetting(SettingsFile.KEY_BACKGROUND_SHADER_COMPILING, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, true);
				break;

			case 2:
				disableSpecializedShaders = new BooleanSetting(SettingsFile.KEY_DISABLE_SPECIALIZED_SHADERS, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, true);
				backgroundShaderCompilation = new BooleanSetting(SettingsFile.KEY_BACKGROUND_SHADER_COMPILING, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, false);
				break;
		}

		mView.putSetting(disableSpecializedShaders);
		mView.putSetting(backgroundShaderCompilation);
	}

	public void putExtensionSetting(int which, int wiimoteNumber)
	{
		StringSetting extension = new StringSetting(SettingsFile.KEY_WIIMOTE_EXTENSION, SettingsFile.SECTION_WIIMOTE + wiimoteNumber,
				SettingsFile.SETTINGS_WIIMOTE, mContext.getResources().getStringArray(R.array.wiimoteExtensionsEntries)[which]);
		mView.putSetting(extension);
	}
}
