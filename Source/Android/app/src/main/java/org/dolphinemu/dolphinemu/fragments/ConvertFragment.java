// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments;

import android.app.Activity;
import android.app.ProgressDialog;
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
import android.widget.Spinner;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;
import org.dolphinemu.dolphinemu.services.GameFileCacheManager;
import org.dolphinemu.dolphinemu.ui.platform.Platform;

import java.io.File;
import java.util.ArrayList;

public class ConvertFragment extends Fragment implements View.OnClickListener
{
  private static class SpinnerValue implements AdapterView.OnItemSelectedListener
  {
    private int mValuesId = -1;
    private int mCurrentPosition = -1;
    private ArrayList<Runnable> mCallbacks = new ArrayList<>();

    @Override
    public void onItemSelected(AdapterView<?> adapterView, View view, int position, long id)
    {
      if (mCurrentPosition != position)
        setPosition(position);
    }

    @Override
    public void onNothingSelected(AdapterView<?> adapterView)
    {
      mCurrentPosition = -1;
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

  private static final int BLOB_TYPE_PLAIN = 0;
  private static final int BLOB_TYPE_GCZ = 3;
  private static final int BLOB_TYPE_WIA = 7;
  private static final int BLOB_TYPE_RVZ = 8;

  private static final int COMPRESSION_NONE = 0;
  private static final int COMPRESSION_PURGE = 1;
  private static final int COMPRESSION_BZIP2 = 2;
  private static final int COMPRESSION_LZMA = 3;
  private static final int COMPRESSION_LZMA2 = 4;
  private static final int COMPRESSION_ZSTD = 5;

  private SpinnerValue mFormat = new SpinnerValue();
  private SpinnerValue mBlockSize = new SpinnerValue();
  private SpinnerValue mCompression = new SpinnerValue();
  private SpinnerValue mCompressionLevel = new SpinnerValue();

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
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
          Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_convert, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    populateFormats();
    populateBlockSize();
    populateCompression();
    populateCompressionLevel();
    populateRemoveJunkData();

    mFormat.addCallback(this::populateBlockSize);
    mFormat.addCallback(this::populateCompression);
    mCompression.addCallback(this::populateCompressionLevel);
    mFormat.addCallback(this::populateRemoveJunkData);

    view.findViewById(R.id.button_convert).setOnClickListener(this);

    if (savedInstanceState != null)
    {
      setSpinnerSelection(R.id.spinner_format, mFormat, savedInstanceState.getInt(KEY_FORMAT));
      setSpinnerSelection(R.id.spinner_block_size, mBlockSize,
              savedInstanceState.getInt(KEY_BLOCK_SIZE));
      setSpinnerSelection(R.id.spinner_compression, mCompression,
              savedInstanceState.getInt(KEY_COMPRESSION));
      setSpinnerSelection(R.id.spinner_compression_level, mCompressionLevel,
              savedInstanceState.getInt(KEY_COMPRESSION_LEVEL));

      CheckBox removeJunkData = requireView().findViewById(R.id.checkbox_remove_junk_data);
      removeJunkData.setChecked(savedInstanceState.getBoolean(KEY_REMOVE_JUNK_DATA));
    }
  }

  @Override
  public void onSaveInstanceState(@NonNull Bundle outState)
  {
    outState.putInt(KEY_FORMAT, mFormat.getPosition());
    outState.putInt(KEY_BLOCK_SIZE, mBlockSize.getPosition());
    outState.putInt(KEY_COMPRESSION, mCompression.getPosition());
    outState.putInt(KEY_COMPRESSION_LEVEL, mCompressionLevel.getPosition());

    CheckBox removeJunkData = requireView().findViewById(R.id.checkbox_remove_junk_data);
    outState.putBoolean(KEY_REMOVE_JUNK_DATA, removeJunkData.isChecked());
  }

  private void setSpinnerSelection(int id, SpinnerValue valueWrapper, int i)
  {
    ((Spinner) requireView().findViewById(id)).setSelection(i);
    valueWrapper.setPosition(i);
  }

  @Override
  public void onStop()
  {
    super.onStop();

    mCanceled = true;
    joinThread();
  }

  private Spinner populateSpinner(int spinnerId, int entriesId, int valuesId,
          SpinnerValue valueWrapper)
  {
    Spinner spinner = requireView().findViewById(spinnerId);

    ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(requireContext(),
            entriesId, android.R.layout.simple_spinner_item);
    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    spinner.setAdapter(adapter);

    spinner.setEnabled(spinner.getCount() > 1);

    valueWrapper.populate(valuesId);
    valueWrapper.setPosition(spinner.getSelectedItemPosition());
    spinner.setOnItemSelectedListener(valueWrapper);

    return spinner;
  }

  private Spinner clearSpinner(int spinnerId, SpinnerValue valueWrapper)
  {
    Spinner spinner = requireView().findViewById(spinnerId);

    spinner.setAdapter(null);

    spinner.setEnabled(false);

    valueWrapper.populate(-1);
    valueWrapper.setPosition(-1);
    spinner.setOnItemSelectedListener(valueWrapper);

    return spinner;
  }

  private void populateFormats()
  {
    Spinner spinner = populateSpinner(R.id.spinner_format, R.array.convertFormatEntries,
            R.array.convertFormatValues, mFormat);

    if (gameFile.getBlobType() == BLOB_TYPE_PLAIN)
      spinner.setSelection(spinner.getCount() - 1);
  }

  private void populateBlockSize()
  {
    switch (mFormat.getValue(requireContext()))
    {
      case BLOB_TYPE_GCZ:
        // In the equivalent DolphinQt code, we have some logic for avoiding block sizes that can
        // trigger bugs in Dolphin versions older than 5.0-11893, but it was too annoying to port.
        // TODO: Port it?
        populateSpinner(R.id.spinner_block_size, R.array.convertBlockSizeGczEntries,
                R.array.convertBlockSizeGczValues, mBlockSize);
        break;
      case BLOB_TYPE_WIA:
        populateSpinner(R.id.spinner_block_size, R.array.convertBlockSizeWiaEntries,
                R.array.convertBlockSizeWiaValues, mBlockSize);
        break;
      case BLOB_TYPE_RVZ:
        populateSpinner(R.id.spinner_block_size, R.array.convertBlockSizeRvzEntries,
                R.array.convertBlockSizeRvzValues, mBlockSize).setSelection(2);
        break;
      default:
        clearSpinner(R.id.spinner_block_size, mBlockSize);
    }
  }

  private void populateCompression()
  {
    switch (mFormat.getValue(requireContext()))
    {
      case BLOB_TYPE_GCZ:
        populateSpinner(R.id.spinner_compression, R.array.convertCompressionGczEntries,
                R.array.convertCompressionGczValues, mCompression);
        break;
      case BLOB_TYPE_WIA:
        populateSpinner(R.id.spinner_compression, R.array.convertCompressionWiaEntries,
                R.array.convertCompressionWiaValues, mCompression);
        break;
      case BLOB_TYPE_RVZ:
        populateSpinner(R.id.spinner_compression, R.array.convertCompressionRvzEntries,
                R.array.convertCompressionRvzValues, mCompression).setSelection(4);
        break;
      default:
        clearSpinner(R.id.spinner_compression, mCompression);
    }
  }

  private void populateCompressionLevel()
  {
    switch (mCompression.getValueOr(requireContext(), COMPRESSION_NONE))
    {
      case COMPRESSION_BZIP2:
      case COMPRESSION_LZMA:
      case COMPRESSION_LZMA2:
        populateSpinner(R.id.spinner_compression_level, R.array.convertCompressionLevelEntries,
                R.array.convertCompressionLevelValues, mCompressionLevel).setSelection(4);
        break;
      case COMPRESSION_ZSTD:
        // TODO: Query DiscIO for the supported compression levels, like we do in DolphinQt?
        populateSpinner(R.id.spinner_compression_level, R.array.convertCompressionLevelZstdEntries,
                R.array.convertCompressionLevelZstdValues, mCompressionLevel).setSelection(4);
        break;
      default:
        clearSpinner(R.id.spinner_compression_level, mCompressionLevel);
    }
  }

  private void populateRemoveJunkData()
  {
    boolean scrubbingAllowed = mFormat.getValue(requireContext()) != BLOB_TYPE_RVZ &&
            !gameFile.isDatelDisc();

    CheckBox removeJunkData = requireView().findViewById(R.id.checkbox_remove_junk_data);
    removeJunkData.setEnabled(scrubbingAllowed);
    if (!scrubbingAllowed)
      removeJunkData.setChecked(false);
  }

  private boolean getRemoveJunkData()
  {
    CheckBox checkBoxScrub = requireView().findViewById(R.id.checkbox_remove_junk_data);
    return checkBoxScrub.isChecked();
  }

  @Override
  public void onClick(View view)
  {
    boolean scrub = getRemoveJunkData();
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

    if (scrub && format == BLOB_TYPE_PLAIN)
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
      AlertDialog.Builder builder = new AlertDialog.Builder(context);
      builder.setMessage(warning_text)
              .setPositiveButton(R.string.yes, (dialog, i) -> action.run())
              .setNegativeButton(R.string.no, null);
      AlertDialog alert = builder.create();
      alert.show();
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
      case BLOB_TYPE_PLAIN:
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

    // For some reason, setting R.style.DolphinDialogBase as the theme here gives us white text
    // on a white background when the device is set to dark mode, so let's not set a theme.
    ProgressDialog progressDialog = new ProgressDialog(context);

    progressDialog.setTitle(R.string.convert_converting);

    progressDialog.setIndeterminate(false);
    progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
    progressDialog.setMax(PROGRESS_RESOLUTION);

    progressDialog.setCancelable(true);
    progressDialog.setOnCancelListener((dialog) -> mCanceled = true);

    progressDialog.show();

    mThread = new Thread(() ->
    {
      boolean success = NativeLibrary.ConvertDiscImage(gameFile.getPath(), outPath,
              gameFile.getPlatform(), mFormat.getValue(context), mBlockSize.getValueOr(context, 0),
              mCompression.getValueOr(context, 0), mCompressionLevel.getValueOr(context, 0),
              getRemoveJunkData(), (text, completion) ->
              {
                requireActivity().runOnUiThread(() ->
                {
                  progressDialog.setMessage(text);
                  progressDialog.setProgress((int) (completion * PROGRESS_RESOLUTION));
                });

                return !mCanceled;
              });

      if (!mCanceled)
      {
        requireActivity().runOnUiThread(() ->
        {
          progressDialog.dismiss();

          AlertDialog.Builder builder = new AlertDialog.Builder(context);
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
          AlertDialog alert = builder.create();
          alert.show();
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
