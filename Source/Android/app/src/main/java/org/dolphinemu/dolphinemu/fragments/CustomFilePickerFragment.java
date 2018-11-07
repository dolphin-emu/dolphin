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

  @Override public void onActivityCreated(Bundle savedInstanceState)
  {
    super.onActivityCreated(savedInstanceState);

    if (mode == MODE_DIR)
    {
      TextView ok = getActivity().findViewById(R.id.nnf_button_ok);
      ok.setText(R.string.select_dir);

      TextView cancel = getActivity().findViewById(R.id.nnf_button_cancel);
      cancel.setVisibility(View.GONE);
    }
  }
}
