package org.dolphinemu.dolphinemu.ui.files;


public interface AddDirectoryView
{
	void showFileFragment(String path, boolean addToStack);

	/**
	 * Called by a contained Fragment in order to tell the Activity what the
	 * currently displayed path is. (This way, the Activity doesn't need to
	 * keep trying to determine what the parent folder was when popping its
	 * back stack of folders.)
	 *
	 * @param path The path to the currently displayed directory.
	 */
	void setPath(String path);

	void onFolderClick(String path);

	void onAddDirectoryClick(String path);

	void popBackStack();

	void finish();

	void showToastMessage(String message);

	void finishSucccessfully();
}
