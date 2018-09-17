/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package com.nononsenseapps.filepicker;


import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import org.dolphinemu.dolphinemu.R;

public abstract class NewItemFragment extends DialogFragment {

    private OnNewFolderListener listener = null;

    public NewItemFragment() {
        super();
    }

    public void setListener(@Nullable final OnNewFolderListener listener) {
        this.listener = listener;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setView(R.layout.nnf_dialog_folder_name)
                .setTitle(R.string.nnf_new_folder)
                .setNegativeButton(R.string.nnf_new_folder_cancel,
                        null)
                .setPositiveButton(R.string.nnf_new_folder_ok,
                        null);

        final AlertDialog dialog = builder.create();

        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow(DialogInterface dialog1) {
                final AlertDialog dialog = (AlertDialog) dialog1;
                final EditText editText = (EditText) dialog.findViewById(R.id.edit_text);

                if (editText == null) {
                    throw new NullPointerException("Could not find an edit text in the dialog");
                }

                Button cancel = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
                cancel.setOnClickListener(new View.OnClickListener() {

                    @Override
                    public void onClick(View view) {
                        dialog.cancel();
                    }
                });

                final Button ok = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
                // Start disabled
                ok.setEnabled(false);
                ok.setOnClickListener(new View.OnClickListener() {

                    @Override
                    public void onClick(View view) {
                        String itemName = editText.getText().toString();
                        if (validateName(itemName)) {
                            if (listener != null) {
                                listener.onNewFolder(itemName);
                            }
                            dialog.dismiss();
                        }
                    }
                });

                editText.addTextChangedListener(new TextWatcher() {
                    @Override
                    public void beforeTextChanged(final CharSequence s, final int start,
                                                  final int count, final int after) {
                    }

                    @Override
                    public void onTextChanged(final CharSequence s, final int start,
                                              final int before, final int count) {
                    }

                    @Override
                    public void afterTextChanged(final Editable s) {
                        ok.setEnabled(validateName(s.toString()));
                    }
                });
            }
        });


        return dialog;
    }

    protected abstract boolean validateName(final String itemName);

    public interface OnNewFolderListener {
        void onNewFolder(@NonNull final String name);
    }
}
