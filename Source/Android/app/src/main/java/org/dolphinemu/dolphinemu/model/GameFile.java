package org.dolphinemu.dolphinemu.model;

import android.os.Environment;

import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class GameFile {
	private long mPointer;  // Do not rename or move without editing the native code

	private GameFile(long pointer) {
		mPointer = pointer;
	}

	@Override
	public native void finalize();

	public native int getPlatform();

	public native String getTitle();

	public native String getDescription();

	public native String getCompany();

	public native int getCountry();

	public native int getRegion();

	public native String getPath();

	public native String getGameId();

	public native int[] getBanner();

	public native int getBannerWidth();

	public native int getBannerHeight();

	public String getCoverPath() {
		return DirectoryInitializationService.getCoverDirectory() + File.separator + getGameId() + ".png";
	}

	public List<String> getSavedStates() {
		final int NUM_STATES = 10;
		final String statePath = Environment.getExternalStorageDirectory().getPath() +
			"/dolphin-emu/StateSaves/";
		final String gameId = getGameId();
		long lastModified = Long.MAX_VALUE;
		ArrayList<String> savedStates = new ArrayList<>();
		for (int i = 1; i < NUM_STATES; ++i) {
			String filename = String.format("%s%s.s%02d", statePath, gameId, i);
			File stateFile = new File(filename);
			if (stateFile.exists()) {
				if (stateFile.lastModified() < lastModified) {
					savedStates.add(0, filename);
					lastModified = stateFile.lastModified();
				} else {
					savedStates.add(filename);
				}
			}
		}
		return savedStates;
	}

	public String getLastSavedState() {
		final int NUM_STATES = 10;
		final String statePath = Environment.getExternalStorageDirectory().getPath() +
			"/dolphin-emu/StateSaves/";
		final String gameId = getGameId();
		long lastModified = Long.MAX_VALUE;
		String savedState = null;
		for (int i = 1; i < NUM_STATES; ++i) {
			String filename = String.format("%s%s.s%02d", statePath, gameId, i);
			File stateFile = new File(filename);
			if (stateFile.exists()) {
				if (stateFile.lastModified() < lastModified) {
					savedState = filename;
					lastModified = stateFile.lastModified();
				}
			}
		}
		return savedState;
	}

	public String getCustomCoverPath() {
		return getPath().substring(0, getPath().lastIndexOf(".")) + ".cover.png";
	}
}
