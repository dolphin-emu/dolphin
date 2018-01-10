package org.dolphinemu.dolphinemu.fragments;

import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.v4.content.FileProvider;

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
}
