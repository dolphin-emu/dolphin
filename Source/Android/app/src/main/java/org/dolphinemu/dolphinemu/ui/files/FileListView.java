package org.dolphinemu.dolphinemu.ui.files;

import org.dolphinemu.dolphinemu.model.FileListItem;

import java.util.List;

public interface FileListView
{
	void showFilesList(List<FileListItem> fileList);

	void onItemClick(String path);

	void onEmptyFolderClick();

	void onBadFolderClick();

	void onFolderClick(String path);

	void onAddDirectoryClick(String path);
}
