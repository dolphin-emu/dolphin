package org.dolphinemu.dolphinemu.dialogs;

import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import com.nononsenseapps.filepicker.DividerItemDecoration;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.features.settings.model.Setting;
import org.dolphinemu.dolphinemu.features.settings.model.Settings;
import org.dolphinemu.dolphinemu.features.settings.model.view.SettingsItem;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsAdapter;
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsFragmentView;

import java.util.ArrayList;

public class RunningSettingDialog extends DialogFragment implements SettingsFragmentView {

	public static RunningSettingDialog newInstance() {
		return new RunningSettingDialog();
	}

	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
		ViewGroup contents = (ViewGroup) getActivity().getLayoutInflater().inflate(R.layout.dialog_running_settings, null);

		int columns = 2;
		Drawable lineDivider = getContext().getDrawable(R.drawable.line_divider);
		RecyclerView recyclerView = contents.findViewById(R.id.list_settings);
		RecyclerView.LayoutManager layoutManager = new GridLayoutManager(getContext(), columns);
		recyclerView.setLayoutManager(layoutManager);
		recyclerView.setAdapter(new SettingsAdapter(this, getContext()));
		recyclerView.addItemDecoration(new DividerItemDecoration(lineDivider));

		builder.setView(contents);
		return builder.create();
	}

	public void onSettingsFileLoaded(Settings settings) {

	}

	public void passSettingsToActivity(Settings settings) {

	}

	public void showSettingsList(ArrayList<SettingsItem> settingsList) {

	}

	public void loadDefaultSettings() {

	}

	public void loadSubMenu(MenuTag menuKey) {

	}

	public void showToastMessage(String message) {

	}

	public void putSetting(Setting setting) {

	}

	public void onSettingChanged() {

	}

	public void onGcPadSettingChanged(MenuTag menuTag, int value) {

	}

	public void onWiimoteSettingChanged(MenuTag menuTag, int value) {

	}

	public void onExtensionSettingChanged(MenuTag menuTag, int value) {

	}
}
