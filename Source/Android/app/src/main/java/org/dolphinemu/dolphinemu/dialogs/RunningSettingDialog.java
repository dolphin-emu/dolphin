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

import com.nononsenseapps.filepicker.DividerItemDecoration;
import org.dolphinemu.dolphinemu.R;
import java.util.ArrayList;

public class RunningSettingDialog extends DialogFragment {

	public class SettingsItem {
		public static final int TYPE_CHECKBOX = 1;
		public static final int TYPE_RADIO_BUTTON = 2;
		public static final int TYPE_SLIDER = 3;

		private String mSetting;
		private int mNameId;
		private int mType;

		public int getType() {
			return mType;
		}
	}

	public class SettingViewHolder extends RecyclerView.ViewHolder {
		public SettingViewHolder(View itemView, int viewType) {
			super(itemView);
		}
	}

	public class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder> {
		private ArrayList<SettingsItem> mSettings;

		public SettingsAdapter() {
			mSettings = new ArrayList<>();
		}

		@Override
		public SettingViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
			View itemView = null;
			LayoutInflater inflater = LayoutInflater.from(parent.getContext());
			switch (viewType) {
				case SettingsItem.TYPE_CHECKBOX:
					itemView = inflater.inflate(R.layout.list_item_running_checkbox, parent, false);
					break;
				case SettingsItem.TYPE_RADIO_BUTTON:
					itemView = inflater.inflate(R.layout.list_item_running_radiobutton, parent, false);
					break;
				case SettingsItem.TYPE_SLIDER:
					itemView = inflater.inflate(R.layout.list_item_running_seekbar, parent, false);
					break;
			}
			return new SettingViewHolder(itemView, viewType);
		}

		@Override
		public int getItemCount() {
			return mSettings.size();
		}

		@Override
		public int getItemViewType(int position) {
			return mSettings.get(position).getType();
		}

		@Override
		public void onBindViewHolder(@NonNull SettingViewHolder holder, int position) {

		}

		public void setSettings(ArrayList<SettingsItem> settings) {
			mSettings = settings;
			notifyDataSetChanged();
		}
	}

	public static RunningSettingDialog newInstance() {
		return new RunningSettingDialog();
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater().inflate(R.layout.dialog_running_settings, null);

		int columns = 2;
		SettingsAdapter adapter = new SettingsAdapter();
		Drawable lineDivider = getContext().getDrawable(R.drawable.line_divider);
		RecyclerView recyclerView = contents.findViewById(R.id.list_settings);
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getContext(), columns);
		recyclerView.setLayoutManager(layoutManager);
		recyclerView.setAdapter(adapter);
		recyclerView.addItemDecoration(new DividerItemDecoration(lineDivider));

		// show settings
		adapter.setSettings(getSettings());

		builder.setView(contents);
		return builder.create();
	}

	private ArrayList<SettingsItem> getSettings() {
		return null;
	}

}
