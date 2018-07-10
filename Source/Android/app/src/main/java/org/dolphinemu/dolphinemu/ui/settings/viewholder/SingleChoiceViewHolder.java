package org.dolphinemu.dolphinemu.ui.settings.viewholder;

import android.view.View;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.model.settings.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.ui.settings.SettingsAdapter;

public final class SingleChoiceViewHolder extends SettingViewHolder
{
	private SettingsItem mItem;

	private TextView mTextSettingName;
	private TextView mTextSettingDescription;

	public SingleChoiceViewHolder(View itemView, SettingsAdapter adapter)
	{
		super(itemView, adapter);
	}

	@Override
	protected void findViews(View root)
	{
		mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
		mTextSettingDescription = (TextView) root.findViewById(R.id.text_setting_description);
	}

	@Override
	public void bind(SettingsItem item)
	{
		mItem = item;

		mTextSettingName.setText(item.getNameId());

		switch (item.getDescriptionId())
		{
			case R.string.dynamic_cpu_core:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.cpu_core_interpreter);
						break;

					case 5:
						mTextSettingDescription.setText(R.string.cpu_core_cached_interpreter);
						break;

					case 4:
						mTextSettingDescription.setText(R.string.cpu_core_jit_arm64);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.cpu_core_jit);
						break;
				}
				break;

			case R.string.dynamic_system_language:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.gamecube_system_language_english);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.gamecube_system_language_german);
						break;

					case 2:
						mTextSettingDescription.setText(R.string.gamecube_system_language_french);
						break;

					case 3:
						mTextSettingDescription.setText(R.string.gamecube_system_language_spanish);
						break;

					case 4:
						mTextSettingDescription.setText(R.string.gamecube_system_language_italian);
						break;

					case 5:
						mTextSettingDescription.setText(R.string.gamecube_system_language_dutch);
						break;
				}
				break;

			case R.string.dynamic_gamecube_slot:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 255:
						mTextSettingDescription.setText(R.string.slot_device_nothing);
						break;

					case 0:
						mTextSettingDescription.setText(R.string.slot_device_dummy);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.slot_device_memory_card);
						break;

					case 8:
						mTextSettingDescription.setText(R.string.slot_device_gci_folder);
						break;
				}
				break;

			case R.string.dynamic_video_backend:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.video_backend_opengl);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.video_backend_vulkan);
						break;

					case 2:
						mTextSettingDescription.setText(R.string.video_backend_software);
						break;

					case 3:
						mTextSettingDescription.setText(R.string.video_backend_null);
						break;
				}
				break;

			case R.string.dynamic_shader_compilation_mode:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.shader_compilation_sync_description);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.shader_compilation_sync_uber_description);
						break;

					case 2:
						mTextSettingDescription.setText(R.string.shader_compilation_async_uber_description);
						break;

					case 3:
						mTextSettingDescription.setText(R.string.shader_compilation_async_skip_description);
						break;
				}
				break;

			case R.string.dynamic_aspect_ratio:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.aspect_ratio_auto);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.aspect_ratio_force_16_9);
						break;

					case 2:
						mTextSettingDescription.setText(R.string.aspect_ratio_force_4_3);
						break;

					case 3:
						mTextSettingDescription.setText(R.string.aspect_ratio_stretch);
						break;
				}
				break;

			case R.string.dynamic_stereoscopy_mode:
				switch (((SingleChoiceSetting) item).getSelectedValue())
				{
					case 0:
						mTextSettingDescription.setText(R.string.stereoscopy_mode_off);
						break;

					case 1:
						mTextSettingDescription.setText(R.string.stereoscopy_mode_side_side);
						break;

					case 2:
						mTextSettingDescription.setText(R.string.stereoscopy_mode_top_bottom);
						break;

					case 3:
						mTextSettingDescription.setText(R.string.stereoscopy_mode_anaglyph);
						break;
				}
				break;

			case 0:
				break;

			default:
				mTextSettingDescription.setText(item.getDescriptionId());
				break;
		}
	}

	@Override
	public void onClick(View clicked)
	{
		if (mItem instanceof SingleChoiceSetting)
		{
			getAdapter().onSingleChoiceClick((SingleChoiceSetting) mItem);
		}
		else if (mItem instanceof StringSingleChoiceSetting)
		{
			getAdapter().onStringSingleChoiceClick((StringSingleChoiceSetting) mItem);
		}
	}
}
