/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import java.util.ArrayList;
import java.util.List;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.utils.CPUHelper;

import android.app.ListFragment;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

/**
 * {@link ListFragment class that is responsible
 * for displaying information related to the CPU.
 */
public final class CPUInfoFragment extends ListFragment
{
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		ListView rootView = (ListView) inflater.inflate(R.layout.gamelist_listview, container, false);
		List<AboutFragmentItem> items = new ArrayList<AboutFragmentItem>();

		CPUHelper cpuHelper = new CPUHelper(getActivity());

		items.add(new AboutFragmentItem(getString(R.string.cpu_info), cpuHelper.getProcessorInfo()));
		items.add(new AboutFragmentItem(getString(R.string.cpu_type), cpuHelper.getProcessorType()));
		items.add(new AboutFragmentItem(getString(R.string.cpu_abi_one), Build.CPU_ABI));
		items.add(new AboutFragmentItem(getString(R.string.cpu_abi_two), Build.CPU_ABI2));
		items.add(new AboutFragmentItem(getString(R.string.num_cores), Integer.toString(cpuHelper.getNumCores())));
		items.add(new AboutFragmentItem(getString(R.string.cpu_features), cpuHelper.getFeatures()));
		items.add(new AboutFragmentItem(getString(R.string.cpu_hardware), Build.HARDWARE));
		if (CPUHelper.isARM())
			items.add(new AboutFragmentItem(getString(R.string.cpu_implementer), cpuHelper.getImplementer()));

		AboutInfoFragmentAdapter adapter = new AboutInfoFragmentAdapter(getActivity(), R.layout.about_layout, items);
		rootView.setAdapter(adapter);

		return rootView;
	}
}
