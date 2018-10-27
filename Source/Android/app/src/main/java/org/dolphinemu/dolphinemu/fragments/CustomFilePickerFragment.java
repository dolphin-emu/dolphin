package org.dolphinemu.dolphinemu.fragments;

import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.content.FileProvider;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.dolphinemu.dolphinemu.R;

import com.nononsenseapps.filepicker.FilePickerFragment;

import java.io.File;

public class CustomFilePickerFragment extends FilePickerFragment
{
  @NonNull
  @Override
  public Uri toUri(@NonNull final File file)
  {
    return FileProvider
            .getUriForFile(getContext(),
                    getContext().getApplicationContext().getPackageName() + ".filesprovider",
                    file);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View view = super.onCreateView(inflater, container, savedInstanceState);
    if (view == null)
      return null;

    TextView ok = view.findViewById(R.id.nnf_button_ok);
    TextView cancel = view.findViewById(R.id.nnf_button_cancel);

    ok.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
    cancel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);

    ok.setAllCaps(false);
    cancel.setAllCaps(false);
    ok.setText(R.string.select_dir);
    return view;
  }
}
