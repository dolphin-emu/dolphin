package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import com.nononsenseapps.filepicker.DividerItemDecoration;
import org.dolphinemu.dolphinemu.R;
import java.util.ArrayList;

public class RunningSettingDialog extends DialogFragment {

	public class SettingsItem {
		public static final int TYPE_CHECKBOX = 1;
		public static final int TYPE_SINGLE_CHOICE = 2;
		public static final int TYPE_SLIDER = 3;
	}

	public class SettingViewHolder extends RecyclerView.ViewHolder {
		public SettingViewHolder(View itemView) {
			super(itemView);
		}
	}

	public class SettingsAdapter extends RecyclerView.Adapter<SettingViewHolder> {
		private ArrayList<SettingsItem> mSettings;

		public SettingsAdapter() {
		}

		@Override
		public SettingViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
			return null;
		}

		@Override
		public int getItemCount() {
			return 0;
		}

		@Override
		public int getItemViewType(int position) {
			return position;
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
