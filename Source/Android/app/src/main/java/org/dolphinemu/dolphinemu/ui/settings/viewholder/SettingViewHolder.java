package org.dolphinemu.dolphinemu.ui.settings.viewholder;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.ui.settings.SettingsAdapter;

public abstract class SettingViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
{
	private SettingsAdapter mAdapter;

	public SettingViewHolder(View itemView, SettingsAdapter adapter)
	{
		super(itemView);

		mAdapter = adapter;

		itemView.setOnClickListener(this);

		findViews(itemView);
	}

	protected SettingsAdapter getAdapter()
	{
		return mAdapter;
	}

	protected abstract void findViews(View root);

	public abstract void bind(SettingsItem item);

	public abstract void onClick(View clicked);
}
