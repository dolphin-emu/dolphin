// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.progressindicator.LinearProgressIndicator;
import com.google.android.material.textfield.MaterialAutoCompleteTextView;
import com.google.android.material.textfield.TextInputLayout;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;

import java.io.File;
import java.util.ArrayList;

public class ConvertFragment extends Fragment implements View.OnClickListener
{
  private static class DropdownValue implements AdapterView.OnItemClickListener
  {
    private int mValuesId = -1;
    private int mCurrentPosition = 0;
    private ArrayList<Runnable> mCallbacks = new ArrayList<>();

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id)
    {
      if (mCurrentPosition != position)
        setPosition(position);
    }

    int getPosition()
    {
      return mCurrentPosition;
    }

    void setPosition(int position)
    {
      mCurrentPosition = position;
      for (Runnable callback : mCallbacks)
        callback.run();
    }

    void populate(int valuesId)
    {
      mValuesId = valuesId;
    }

    boolean hasValues()
    {
      return mValuesId != -1;
    }

    int getValue(Context context)
    {
      return context.getResources().getIntArray(mValuesId)[mCurrentPosition];
    }

    int getValueOr(Context context, int defaultValue)
    {
      return hasValues() ? getValue(context) : defaultValue;
    }

    void addCallback(Runnable callback)
    {
      mCallbacks.add(callback);
    }
  }

  private static final String ARG_GAME_PATH = "game_path";

  private static final String KEY_FORMAT = "convert_format";
  private static final String KEY_BLOCK_SIZE = "convert_block_size";
  private static final String KEY_COMPRESSION = "convert_compression";
  private static final String KEY_COMPRESSION_LEVEL = "convert_compression_level";
  private static final String KEY_REMOVE_JUNK_DATA = "remove_junk_data";

  private static final int REQUEST_CODE_SAVE_FILE = 0;

  private static final int BLOB_TYPE_ISO = 0;
  private static final int BLOB_TYPE_GCZ = 3;
  private static final int BLOB_TYPE_WIA = 7;
  private static final int BLOB_TYPE_RVZ = 8;

  private static final int COMPRESSION_NONE = 0;
  private static final int COMPRESSION_PURGE = 1;
  private static final int COMPRESSION_BZIP2 = 2;
  private static final int COMPRESSION_LZMA = 3;
  private static final int COMPRESSION_LZMA2 = 4;
  private static final int COMPRESSION_ZSTD = 5;

  private DropdownValue mFormat = new DropdownValue();
  private DropdownValue mBlockSize = new DropdownValue();
  private DropdownValue mCompression = new DropdownValue();
  private DropdownValue mCompressionLevel = new DropdownValue();

  private TextInputLayout mFormatLayout;
  private MaterialAutoCompleteTextView mFormatDropdown;
  private TextInputLayout mBlockSizeLayout;
  private MaterialAutoCompleteTextView mBlockSizeDropdown;
  private TextInputLayout mCompressionLayout;
  private MaterialAutoCompleteTextView mCompressionDropdown;
  private TextInputLayout mCompressionLevelLayout;
  private MaterialAutoCompleteTextView mCompressionLevelDropdown;
  private CheckBox mRemoveJunkDataCheckbox;

  private GameFile gameFile;

  private volatile boolean mCanceled;
  private volatile Thread mThread = null;

  public static ConvertFragment newInstance(String gamePath)
  {
    Bundle args = new Bundle();
    args.putString(ARG_GAME_PATH, gamePath);

    ConvertFragment fragment = new ConvertFragment();
    fragment.setArguments(args);
    return fragment;
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    gameFile = GameFileCacheManager.addOrGet(requireArguments().getString(ARG_GAME_PATH));
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_convert, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    findViews();

    // TODO: Remove workaround for text filtering issue in material components when fixed
    // https://github.com/material-components/material-components-android/issues/1464
    mFormatDropdown.setSaveEnabled(false);
    mBlockSizeDropdown.setSaveEnabled(false);
    mCompressionDropdown.setSaveEnabled(false);
    mCompressionLevelDropdown.setSaveEnabled(false);

    populateFormats();
    populateBlockSize();
    populateCompression();
    populateCompressionLevel();
    populateRemoveJunkData();

    mFormat.addCallback(this::populateBlockSize);
    mFormat.addCallback(this::populateCompression);
    mFormat.addCallback(this::populateCompressionLevel);
    mCompression.addCallback(this::populateCompressionLevel);
    mFormat.addCallback(this::populateRemoveJunkData);

    view.findViewById(R.id.button_convert).setOnClickListener(this);

    if (savedInstanceState != null)
    {
      setDropdownSelection(mFormatDropdown, mFormat, savedInstanceState.getInt(KEY_FORMAT));
      setDropdownSelection(mBlockSizeDropdown, mBlockSize,
              savedInstanceState.getInt(KEY_BLOCK_SIZE));
      setDropdownSelection(mCompressionDropdown, mCompression,
              savedInstanceState.getInt(KEY_COMPRESSION));
      setDropdownSelection(mCompressionLevelDropdown, mCompressionLevel,
              savedInstanceState.getInt(KEY_COMPRESSION_LEVEL));

      mRemoveJunkDataCheckbox.setChecked(savedInstanceState.getBoolean(KEY_REMOVE_JUNK_DATA));
    }
  }

  private void findViews()
  {
    mFormatLayout = requireView().findViewById(R.id.format);
    mFormatDropdown = requireView().findViewById(R.id.dropdown_format);
    mBlockSizeLayout = requireView().findViewById(R.id.block_size);
    mBlockSizeDropdown = requireView().findViewById(R.id.dropdown_block_size);
    mCompressionLayout = requireView().findViewById(R.id.compression);
    mCompressionDropdown = requireView().findViewById(R.id.dropdown_compression);
    mCompressionLevelLayout = requireView().findViewById(R.id.compression_level);
    mCompressionLevelDropdown = requireView().findViewById(R.id.dropdown_compression_level);
    mRemoveJunkDataCheckbox = requireView().findViewById(R.id.checkbox_remove_junk_data);
  }

  @Override
  public void onSaveInstanceState(@NonNull Bundle outState)
  {
    outState.putInt(KEY_FORMAT, mFormat.getPosition());
    outState.putInt(KEY_BLOCK_SIZE, mBlockSize.getPosition());
    outState.putInt(KEY_COMPRESSION, mCompression.getPosition());
    outState.putInt(KEY_COMPRESSION_LEVEL, mCompressionLevel.getPosition());

    outState.putBoolean(KEY_REMOVE_JUNK_DATA, mRemoveJunkDataCheckbox.isChecked());
  }

  private void setDropdownSelection(MaterialAutoCompleteTextView dropdown,
          DropdownValue valueWrapper, int selection)
  {
    if (dropdown.getAdapter() != null)
    {
      dropdown.setText(dropdown.getAdapter().getItem(selection).toString(), false);
    }
    valueWrapper.setPosition(selection);
  }

  @Override
  public void onStop()
  {
    super.onStop();

    mCanceled = true;
    joinThread();
  }

  private void populateDropdown(TextInputLayout layout, MaterialAutoCompleteTextView dropdown,
          int entriesId, int valuesId, DropdownValue valueWrapper)
  {
    ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(requireContext(),
            entriesId, R.layout.support_simple_spinner_dropdown_item);
    dropdown.setAdapter(adapter);

    valueWrapper.populate(valuesId);
    dropdown.setOnItemClickListener(valueWrapper);

    layout.setEnabled(adapter.getCount() > 1);
  }

  private void clearDropdown(TextInputLayout layout, MaterialAutoCompleteTextView dropdown,
          DropdownValue valueWrapper)
  {
    dropdown.setAdapter(null);
    layout.setEnabled(false);

    valueWrapper.populate(-1);
    valueWrapper.setPosition(0);
    dropdown.setText(null, false);
    dropdown.setOnItemClickListener(valueWrapper);
  }

  private void populateFormats()
  {
    populateDropdown(mFormatLayout, mFormatDropdown, R.array.convertFormatEntries,
            R.array.convertFormatValues, mFormat);
    if (gameFile.getBlobType() == BLOB_TYPE_ISO)
    {
      setDropdownSelection(mFormatDropdown, mFormat, mFormatDropdown.getAdapter().getCount() - 1);
    }
    mFormatDropdown.setText(mFormatDropdown.getAdapter().getItem(mFormat.getPosition()).toString(),
            false);
  }

  private void populateBlockSize()
  {
    switch (mFormat.getValue(requireContext()))
    {
      case BLOB_TYPE_GCZ:
        // In the equivalent DolphinQt code, we have some logic for avoiding block sizes that can
        // trigger bugs in Dolphin versions older than 5.0-11893, but it was too annoying to port.
        // TODO: Port it?
        populateDropdown(mBlockSizeLayout, mBlockSizeDropdown, R.array.convertBlockSizeGczEntries,
                R.array.convertBlockSizeGczValues, mBlockSize);
        mBlockSize.setPosition(0);
        mBlockSizeDropdown.setText(mBlockSizeDropdown.getAdapter().getItem(0).toString(), false);
        break;
      case BLOB_TYPE_WIA:
        populateDropdown(mBlockSizeLayout, mBlockSizeDropdown, R.array.convertBlockSizeWiaEntries,
                R.array.convertBlockSizeWiaValues, mBlockSize);
        mBlockSize.setPosition(0);
        mBlockSizeDropdown.setText(mBlockSizeDropdown.getAdapter().getItem(0).toString(), false);
        break;
      case BLOB_TYPE_RVZ:
        populateDropdown(mBlockSizeLayout, mBlockSizeDropdown, R.array.convertBlockSizeRvzEntries,
                R.array.convertBlockSizeRvzValues, mBlockSize);
        mBlockSize.setPosition(2);
        mBlockSizeDropdown.setText(mBlockSizeDropdown.getAdapter().getItem(2).toString(), false);
        break;
      default:
        clearDropdown(mBlockSizeLayout, mBlockSizeDropdown, mBlockSize);
    }
  }

  private void populateCompression()
  {
    switch (mFormat.getValue(requireContext()))
    {
      case BLOB_TYPE_GCZ:
        populateDropdown(mCompressionLayout, mCompressionDropdown,
                R.array.convertCompressionGczEntries, R.array.convertCompressionGczValues,
                mCompression);
        mCompression.setPosition(0);
        mCompressionDropdown.setText(mCompressionDropdown.getAdapter().getItem(0).toString(),
                false);
        break;
      case BLOB_TYPE_WIA:
        populateDropdown(mCompressionLayout, mCompressionDropdown,
                R.array.convertCompressionWiaEntries, R.array.convertCompressionWiaValues,
                mCompression);
        mCompression.setPosition(0);
        mCompressionDropdown.setText(mCompressionDropdown.getAdapter().getItem(0).toString(),
                false);
        break;
      case BLOB_TYPE_RVZ:
        populateDropdown(mCompressionLayout, mCompressionDropdown,
                R.array.convertCompressionRvzEntries, R.array.convertCompressionRvzValues,
                mCompression);
        mCompression.setPosition(4);
        mCompressionDropdown.setText(mCompressionDropdown.getAdapter().getItem(4).toString(),
                false);
        break;
      default:
        clearDropdown(mCompressionLayout, mCompressionDropdown, mCompression);
    }
  }

  private void populateCompressionLevel()
  {
    switch (mCompression.getValueOr(requireContext(), COMPRESSION_NONE))
    {
      case COMPRESSION_BZIP2:
      case COMPRESSION_LZMA:
      case COMPRESSION_LZMA2:
        populateDropdown(mCompressionLevelLayout, mCompressionLevelDropdown,
                R.array.convertCompressionLevelEntries, R.array.convertCompressionLevelValues,
                mCompressionLevel);
        mCompressionLevel.setPosition(4);
        mCompressionLevelDropdown.setText(
                mCompressionLevelDropdown.getAdapter().getItem(4).toString(), false);
        break;
      case COMPRESSION_ZSTD:
        // TODO: Query DiscIO for the supported compression levels, like we do in DolphinQt?
        populateDropdown(mCompressionLevelLayout, mCompressionLevelDropdown,
                R.array.convertCompressionLevelZstdEntries,
                R.array.convertCompressionLevelZstdValues, mCompressionLevel);
        mCompressionLevel.setPosition(4);
        mCompressionLevelDropdown.setText(
                mCompressionLevelDropdown.getAdapter().getItem(4).toString(), false);
        break;
      default:
        clearDropdown(mCompressionLevelLayout, mCompressionLevelDropdown, mCompressionLevel);
    }
  }

  private void populateRemoveJunkData()
  {
    boolean scrubbingAllowed = mFormat.getValue(requireContext()) != BLOB_TYPE_RVZ &&
            !gameFile.isDatelDisc();

    mRemoveJunkDataCheckbox.setEnabled(scrubbingAllowed);
    if (!scrubbingAllowed)
      mRemoveJunkDataCheckbox.setChecked(false);
  }

  @Override
  public void onClick(View view)
  {
    boolean scrub = mRemoveJunkDataCheckbox.isChecked();
    int format = mFormat.getValue(requireContext());

    Runnable action = this::showSavePrompt;

    if (gameFile.isNKit())
    {
      action = addAreYouSureDialog(action, R.string.convert_warning_nkit);
    }

    if (!scrub && format == BLOB_TYPE_GCZ && !gameFile.isDatelDisc() &&
            gameFile.getPlatform() == Platform.WII.toInt())
    {
      action = addAreYouSureDialog(action, R.string.convert_warning_gcz);
    }

    if (scrub && format == BLOB_TYPE_ISO)
    {
      action = addAreYouSureDialog(action, R.string.convert_warning_iso);
    }

    action.run();
  }

  private Runnable addAreYouSureDialog(Runnable action, @StringRes int warning_text)
  {
    return () ->
    {
      Context context = requireContext();
      new MaterialAlertDialogBuilder(context)
              .setMessage(warning_text)
              .setPositiveButton(R.string.yes, (dialog, i) -> action.run())
              .setNegativeButton(R.string.no, null)
              .show();
    };
  }

  private void showSavePrompt()
  {
    String originalPath = gameFile.getPath();

    StringBuilder filename = new StringBuilder(new File(originalPath).getName());
    int dotIndex = filename.lastIndexOf(".");
    if (dotIndex != -1)
      filename.setLength(dotIndex);
    switch (mFormat.getValue(requireContext()))
    {
      case BLOB_TYPE_ISO:
        filename.append(".iso");
        break;
      case BLOB_TYPE_GCZ:
        filename.append(".gcz");
        break;
      case BLOB_TYPE_WIA:
        filename.append(".wia");
        break;
      case BLOB_TYPE_RVZ:
        filename.append(".rvz");
        break;
    }

    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("application/octet-stream");
    intent.putExtra(Intent.EXTRA_TITLE, filename.toString());
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
      intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, originalPath);
    startActivityForResult(intent, REQUEST_CODE_SAVE_FILE);
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    if (requestCode == REQUEST_CODE_SAVE_FILE && resultCode == Activity.RESULT_OK)
    {
      convert(data.getData().toString());
    }
  }

  private void convert(String outPath)
  {
    final int PROGRESS_RESOLUTION = 1000;

    Context context = requireContext();

    joinThread();

    mCanceled = false;

    View dialogView = getLayoutInflater().inflate(R.layout.dialog_progress, null, false);
    LinearProgressIndicator progressBar = dialogView.findViewById(R.id.update_progress);
    progressBar.setMax(PROGRESS_RESOLUTION);

    AlertDialog progressDialog = new MaterialAlertDialogBuilder(context)
            .setTitle(R.string.convert_converting)
            .setOnCancelListener((dialog) -> mCanceled = true)
            .setNegativeButton(getString(R.string.cancel), (dialog, i) -> dialog.dismiss())
            .setView(dialogView)
            .show();

    mThread = new Thread(() ->
    {
      boolean success = NativeLibrary.ConvertDiscImage(gameFile.getPath(), outPath,
              gameFile.getPlatform(), mFormat.getValue(context), mBlockSize.getValueOr(context, 0),
              mCompression.getValueOr(context, 0), mCompressionLevel.getValueOr(context, 0),
              mRemoveJunkDataCheckbox.isChecked(), (text, completion) ->
              {
                requireActivity().runOnUiThread(() ->
                {
                  progressDialog.setMessage(text);
                  progressBar.setProgress((int) (completion * PROGRESS_RESOLUTION));
                });
                return !mCanceled;
              });

      if (!mCanceled)
      {
        requireActivity().runOnUiThread(() ->
        {
          progressDialog.dismiss();

          MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(context);
          if (success)
          {
            builder.setMessage(R.string.convert_success_message)
                    .setCancelable(false)
                    .setPositiveButton(R.string.ok, (dialog, i) ->
                    {
                      dialog.dismiss();
                      requireActivity().finish();
                    });
          }
          else
          {
            builder.setMessage(R.string.convert_failure_message)
                    .setPositiveButton(R.string.ok, (dialog, i) -> dialog.dismiss());
          }
          builder.show();
        });
      }
    });

    mThread.start();
  }

  private void joinThread()
  {
    if (mThread != null)
    {
      try
      {
        mThread.join();
      }
      catch (InterruptedException ignored)
      {
      }
    }
  }
}
