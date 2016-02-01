package org.dolphinemu.dolphinemu.ui.files;


import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.dolphinemu.dolphinemu.BuildConfig;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.application.injectors.FragmentInjector;
import org.dolphinemu.dolphinemu.model.FileListItem;
import org.dolphinemu.dolphinemu.ui.settings.BaseFragment;

import java.util.List;

import javax.inject.Inject;

public class FileListFragment extends BaseFragment implements FileListView
{
	@Inject
	public FileListFragmentPresenter mPresenter;

	private AddDirectoryView mActivity;

	private FrameLayout mFrameContent;
	private RecyclerView mRecyclerView;

	@Override
	protected void onAttachHelper(Activity activity)
	{
		super.onAttachHelper(activity);

		mActivity = (AddDirectoryActivity) activity;
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		String path = getArguments().getString(ARGUMENT_PATH);

		mPresenter.onCreate(path);
	}

	@Nullable
	@Override
	public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
	{
		return inflater.inflate(R.layout.fragment_file_list, container, false);
	}

	@Override
	public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
	{
		findViews(view);

		LinearLayoutManager manager = new LinearLayoutManager(getActivity());

		mRecyclerView.setLayoutManager(manager);

		mPresenter.onViewCreated();
	}

	@Override
	public void onStart()
	{
		super.onStart();
		mActivity.setPath(mPresenter.getPath());
	}

	@Override
	public void onDetach()
	{
		super.onDetach();
		mActivity = null;
	}

	@Override
	public void showFilesList(List<FileListItem> fileList)
	{
		FileAdapter adapter = new FileAdapter(fileList, this);
		mRecyclerView.setAdapter(adapter);
	}

	@Override
	public void onItemClick(String path)
	{
		mPresenter.onItemClick(path);
	}

	@Override
	public void onEmptyFolderClick()
	{
		showToastMessage(getString(R.string.add_directory_empty_folder));
	}

	@Override
	public void onBadFolderClick()
	{
		showToastMessage(getString(R.string.add_directory_bad_folder));
	}

	@Override
	public void onFolderClick(String path)
	{
		mActivity.onFolderClick(path);
	}

	@Override
	public void onAddDirectoryClick(String path)
	{
		mActivity.onAddDirectoryClick(path);
	}

	@Override
	protected void inject()
	{
		FragmentInjector.inject(this);
	}

	@Override
	protected FrameLayout getContentLayout()
	{
		return mFrameContent;
	}

	@Override
	protected String getTitle()
	{
		return null;
	}

	@Override
	protected String getSubtitle()
	{
		return mPresenter.getPath();
	}

	private void findViews(View root)
	{
		mFrameContent = (FrameLayout) root.findViewById(R.id.frame_content);
		mRecyclerView = (RecyclerView) root.findViewById(R.id.list_files);
	}

	public static final String FRAGMENT_TAG = BuildConfig.APPLICATION_ID + ".fragment.file_list";

	public static final String ARGUMENT_PATH = FRAGMENT_TAG + ".path";

	public static FileListFragment newInstance(String path)
	{
		FileListFragment fragment = new FileListFragment();

		Bundle arguments = new Bundle();
		arguments.putString(ARGUMENT_PATH, path);

		fragment.setArguments(arguments);
		return fragment;
	}
}
