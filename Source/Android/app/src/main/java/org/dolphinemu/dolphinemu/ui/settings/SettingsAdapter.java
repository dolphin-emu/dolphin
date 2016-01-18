package org.dolphinemu.dolphinemu.ui.settings;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.model.settings.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SliderSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.CheckBoxSettingViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.HeaderViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SettingViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SingleChoiceViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SliderViewHolder;
import org.dolphinemu.dolphinemu.ui.settings.viewholder.SubmenuViewHolder;
import org.dolphinemu.dolphinemu.utils.Log;

import java.util.ArrayList;

public class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder>
{
	private SettingsFragmentView mView;
	private Context mContext;
	private ArrayList<SettingsItem> mSettings;

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

	public void onSingleChoiceClick(SingleChoiceSetting item)
	{
		Toast.makeText(mContext, "Single choice item clicked", Toast.LENGTH_SHORT).show();
	}

	public void onSliderClick(SliderSetting item)
	{
		Toast.makeText(mContext, "Slider item clicked", Toast.LENGTH_SHORT).show();
	}

	public void onSubmenuClick(SubmenuSetting item)
	{
		Toast.makeText(mContext, "Submenu item clicked", Toast.LENGTH_SHORT).show();
	}
}
