package org.dolphinemu.dolphinemu;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.FragmentMoreBinding;
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemMenuNotInstalledDialogFragment;
import org.dolphinemu.dolphinemu.features.sysupdate.ui.SystemUpdateViewModel;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.main.MainView;
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner;
import org.dolphinemu.dolphinemu.utils.InsetsHelper;
import org.dolphinemu.dolphinemu.utils.WiiUtils;

import static org.dolphinemu.dolphinemu.ui.main.MainPresenter.*;

public class MoreFragment extends Fragment
{
  private FragmentMoreBinding mBinding;

  public static final MutableLiveData<Boolean> mIsSystemMenuInstalled = new MutableLiveData<>();

  public MoreFragment()
  {
    // Required empty public constructor
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    MainView mainView = ((MainView) requireActivity());
    mBinding = FragmentMoreBinding.inflate(inflater, container, false);

    InsetsHelper.setUpList(requireContext(), mBinding.moreScrollView);

    mBinding.optionOpenFile.setOnClickListener(
            v -> ((MainView) requireActivity()).launchOpenFileActivity(REQUEST_GAME_FILE));

    mBinding.optionInstallWad.setOnClickListener(
            v -> new AfterDirectoryInitializationRunner().runWithLifecycle(getActivity(),
                    () -> mainView.launchOpenFileActivity(REQUEST_WAD_FILE)));

    // Set system menu text when download is complete
    Observer<Boolean> menuInstalledObserver = (isInstalled) -> setSystemMenuText();
    isSystemMenuInstalled().observe(requireActivity(), menuInstalledObserver);

    // Set system menu text at startup
    setSystemMenuText();
    mBinding.optionLoadWiiSystemMenu.setOnClickListener(v -> launchWiiSystemMenu());

    mBinding.optionImportWiiSave.setOnClickListener(
            v -> new AfterDirectoryInitializationRunner().runWithLifecycle(getActivity(),
                    () -> mainView.launchOpenFileActivity(REQUEST_WII_SAVE_FILE)));

    mBinding.optionImportNandBackup.setOnClickListener(
            v -> new AfterDirectoryInitializationRunner().runWithLifecycle(getActivity(),
                    () -> mainView.launchOpenFileActivity(REQUEST_NAND_BIN_FILE)));

    // TODO: CHECK FOR COLLISIONS
    mBinding.optionOnlineSystemUpdate.setOnClickListener(v -> launchOnlineUpdate());

    mBinding.optionRefresh.setOnClickListener(v ->
    {
      mainView.setRefreshing(true);
      GameFileCacheManager.startRescan();
      Toast.makeText(requireContext(), getString(R.string.refreshing), Toast.LENGTH_SHORT).show();
    });

    mBinding.optionSettings.setOnClickListener(
            v -> mainView.launchSettingsActivity(MenuTag.SETTINGS));

    return mBinding.getRoot();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mBinding = null;
  }

  public void launchOnlineUpdate()
  {
    if (WiiUtils.isSystemMenuInstalled())
    {
      SystemUpdateViewModel viewModel =
              new ViewModelProvider(requireActivity()).get(SystemUpdateViewModel.class);
      viewModel.setRegion(-1);
      launchUpdateProgressBarFragment(requireActivity());
    }
    else
    {
      SystemMenuNotInstalledDialogFragment dialogFragment =
              new SystemMenuNotInstalledDialogFragment();
      dialogFragment.show(requireActivity().getSupportFragmentManager(),
              "SystemMenuNotInstalledDialogFragment");
    }
  }

  private void launchWiiSystemMenu()
  {
    new AfterDirectoryInitializationRunner().runWithLifecycle(getActivity(), () ->
    {
      if (WiiUtils.isSystemMenuInstalled())
      {
        EmulationActivity.launchSystemMenu(getActivity());
      }
      else
      {
        SystemMenuNotInstalledDialogFragment dialogFragment =
                new SystemMenuNotInstalledDialogFragment();
        dialogFragment
                .show(requireActivity().getSupportFragmentManager(),
                        "SystemMenuNotInstalledDialogFragment");
      }
    });
  }

  private void setSystemMenuText()
  {
    if (WiiUtils.isSystemMenuInstalled())
    {
      int resId = WiiUtils.isSystemMenuvWii() ?
              R.string.load_vwii_system_menu_installed :
              R.string.load_wii_system_menu_installed;

      mBinding.loadWiiSystemMenuText.setText(getString(resId, WiiUtils.getSystemMenuVersion()));
    }
  }

  private static LiveData<Boolean> isSystemMenuInstalled()
  {
    return mIsSystemMenuInstalled;
  }
}
