// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.ui;

import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.activities.EmulationActivity;
import org.dolphinemu.dolphinemu.databinding.DialogCreateSkylanderBinding;
import org.dolphinemu.dolphinemu.databinding.ListItemSkylanderSlotBinding;
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig;
import org.dolphinemu.dolphinemu.features.skylanders.model.SkylanderPair;

import java.util.ArrayList;
import java.util.List;

public class SkylanderSlotAdapter extends RecyclerView.Adapter<SkylanderSlotAdapter.ViewHolder>
        implements AdapterView.OnItemClickListener
{
  public static class ViewHolder extends RecyclerView.ViewHolder
  {
    public ListItemSkylanderSlotBinding binding;

    public ViewHolder(@NonNull ListItemSkylanderSlotBinding binding)
    {
      super(binding.getRoot());
      this.binding = binding;
    }
  }

  private final List<SkylanderSlot> mSlots;
  private final EmulationActivity mActivity;

  private DialogCreateSkylanderBinding mBinding;

  public SkylanderSlotAdapter(List<SkylanderSlot> slots, EmulationActivity context)
  {
    mSlots = slots;
    mActivity = context;
  }

  @NonNull
  @Override
  public SkylanderSlotAdapter.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    ListItemSkylanderSlotBinding binding =
            ListItemSkylanderSlotBinding.inflate(inflater, parent, false);
    return new ViewHolder(binding);
  }

  @Override
  public void onBindViewHolder(@NonNull ViewHolder holder, int position)
  {
    SkylanderSlot slot = mSlots.get(position);
    holder.binding.textSkylanderName.setText(slot.getLabel());

    holder.binding.buttonClearSkylander.setOnClickListener(v ->
    {
      SkylanderConfig.removeSkylander(slot.getPortalSlot());
      mActivity.clearSkylander(slot.getSlotNum());
    });

    holder.binding.buttonLoadSkylander.setOnClickListener(v ->
    {
      Intent loadSkylander = new Intent(Intent.ACTION_OPEN_DOCUMENT);
      loadSkylander.addCategory(Intent.CATEGORY_OPENABLE);
      loadSkylander.setType("*/*");
      mActivity.setSkylanderData(0, 0, "", position);
      mActivity.startActivityForResult(loadSkylander, EmulationActivity.REQUEST_SKYLANDER_FILE);
    });

    LayoutInflater inflater = LayoutInflater.from(mActivity);
    mBinding = DialogCreateSkylanderBinding.inflate(inflater);

    List<String> skylanderNames = new ArrayList<>(SkylanderConfig.REVERSE_LIST_SKYLANDERS.keySet());
    skylanderNames.sort(String::compareToIgnoreCase);

    mBinding.skylanderDropdown.setAdapter(
            new ArrayAdapter<>(mActivity, R.layout.support_simple_spinner_dropdown_item,
                    skylanderNames));
    mBinding.skylanderDropdown.setOnItemClickListener(this);

    holder.binding.buttonCreateSkylander.setOnClickListener(v ->
    {
      if (mBinding.getRoot().getParent() != null)
      {
        ((ViewGroup) mBinding.getRoot().getParent()).removeAllViews();
      }
      AlertDialog createDialog = new MaterialAlertDialogBuilder(mActivity)
              .setTitle(R.string.create_skylander_title)
              .setView(mBinding.getRoot())
              .setPositiveButton(R.string.create_skylander, null)
              .setNegativeButton(R.string.cancel, null)
              .show();
      createDialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE).setOnClickListener(
              v1 ->
              {
                if (!mBinding.skylanderId.getText().toString().isBlank() &&
                        !mBinding.skylanderVar.getText().toString().isBlank())
                {
                  Intent createSkylander = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                  createSkylander.addCategory(Intent.CATEGORY_OPENABLE);
                  createSkylander.setType("*/*");
                  int id = Integer.parseInt(mBinding.skylanderId.getText().toString());
                  int var = Integer.parseInt(mBinding.skylanderVar.getText().toString());
                  String name = SkylanderConfig.LIST_SKYLANDERS.get(new SkylanderPair(id, var));
                  if (name != null)
                  {
                    createSkylander.putExtra(Intent.EXTRA_TITLE,
                            name + ".sky");
                    mActivity.setSkylanderData(id, var, name, position);
                  }
                  else
                  {
                    createSkylander.putExtra(Intent.EXTRA_TITLE,
                            "Unknown(ID" + id + "Var" + var + ").sky");
                    mActivity.setSkylanderData(id, var, "Unknown", position);
                  }
                  mActivity.startActivityForResult(createSkylander,
                          EmulationActivity.REQUEST_CREATE_SKYLANDER);
                  createDialog.dismiss();
                }
                else
                {
                  Toast.makeText(mActivity, R.string.invalid_skylander,
                          Toast.LENGTH_SHORT).show();
                }
              });
    });

  }

  @Override
  public int getItemCount()
  {
    return mSlots.size();
  }

  @Override
  public void onItemClick(AdapterView<?> parent, View view, int position, long id)
  {
    SkylanderPair skylanderIdVar =
            SkylanderConfig.REVERSE_LIST_SKYLANDERS.get(parent.getItemAtPosition(position));
    mBinding.skylanderId.setText(String.valueOf(skylanderIdVar.getId()));
    mBinding.skylanderVar.setText(String.valueOf(skylanderIdVar.getVar()));
  }
}
